#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "stdbool.h"
#include "assert.h"

#include "utils.h"
#include "logger.h"
#include "nameTable.h"
#include "Context.h"

Node_t *createNode(enum ElemType type, int iVal, double dVal, Node_t *left, Node_t *right) {
    logPrint(L_EXTRA, 0, "ExprTree:Creating node\n");

    Node_t *newNode = CALLOC(1, Node_t);
    newNode->type = type;
    newNode->left  = left;
    newNode->right = right;

    if (left)
        left->parent = newNode;
    if (right)
        right->parent = newNode;

    switch(type) {
        case OPERATOR:
            newNode->value.op = (enum OperatorType) iVal;
            break;
        case IDENTIFIER:
            newNode->value.id = iVal;
            break;
        case NUMBER:
            newNode->value.number = dVal;
            break;
        default:
            assert(0);
    }

    logPrint(L_EXTRA, 0, "ExprTree:Created node[%p]\n", newNode);
    return newNode;
}

Node_t *copyTree(Node_t *node) {
    assert(node);

    logPrint(L_EXTRA, 0, "ExprTree:Copying node[%p]\n", node);

    Node_t *left  = (node->left)  ? copyTree(node->left)  : NULL,
           *right = (node->right) ? copyTree(node->right) : NULL;

    return createNode(node->type, node->value.id, node->value.number, left, right);
}

/// @brief Give node allocated in a stack. Free is not supported
Node_t *getEmptyNode(LangContext_t *context) {
    // if (context->mode != FRONTEND_BACKWARD) {
    //     logPrint(L_ZERO, 1, "Wrong frontend mode; can't allocate node\n");
    //     return NULL;
    // }

    Node_t *node = GET_MEMORY(&context->treeMemory, Node_t);
    if (!node) {
        logPrint(L_ZERO, 1, "Tree stack overflow\nIncrease maxTokens\n");
        return NULL;
    }

    return node;
}

static bool recursiveDumpTree(LangContext_t *context, Node_t *node, bool minified, FILE *dotFile);

bool dumpTree(LangContext_t *context, Node_t *node, bool minified) {
    static size_t dumpCounter = 0;
    dumpCounter++;
    system("mkdir -p " LOGS_DIR "/" DOTS_DIR " " LOGS_DIR "/" IMGS_DIR);

    char buffer[DUMP_BUFFER_SIZE] = "";
    sprintf(buffer, LOGS_DIR "/" DOTS_DIR "/" EXPR_TREE_DUMP_DOT_FORMAT, dumpCounter);
    FILE *dotFile = fopen(buffer, "w");

    fprintf(dotFile, "digraph {\n"
                      "graph [splines=line]\n");

    bool result = recursiveDumpTree(context, node, minified, dotFile);

    fprintf(dotFile, "}\n");
    fclose(dotFile);

    if (!result) return false;

    const char *extension = "svg";
    sprintf(buffer, "dot " LOGS_DIR "/" DOTS_DIR "/" EXPR_TREE_DUMP_DOT_FORMAT
                    " -T%s -o " LOGS_DIR "/" IMGS_DIR "/" EXPR_TREE_DUMP_IMG_FORMAT "%s",
                    dumpCounter, extension, dumpCounter, extension);
    if (system(buffer) != 0)
        return false;

    logPrint(L_ZERO, 0, "<img src=\"" IMGS_DIR "/" EXPR_TREE_DUMP_IMG_FORMAT "%s\" width=76%%>\n<hr>\n", dumpCounter, extension);
    return true;
}

static bool recursiveDumpTree(LangContext_t *context, Node_t *node, bool minified, FILE *dotFile) {
    if (!node) return false;
    if (!dotFile) return false;

    fprintf(dotFile, "\tnode%p [shape = Mrecord, label = \"{", node);

    if (!minified) {
        fprintf(dotFile, "node[%p] | parent[%p] |", node, node->parent);
    }

    const char *fillColor = DEFAULT_NODE_COLOR;

    switch(node->type) {
        case OPERATOR:
            fillColor = OPERATOR_COLOR;
            if (node->value.op == OP_SEP || node->value.op == OP_COMMA)
                fillColor = SEP_COLOR;
            fprintf(dotFile, "TYPE = OPR(%d) | %s | ", node->type,
                operators[node->value.op].dotStr ? operators[node->value.op].dotStr : operators[node->value.op].str);
            break;
        case IDENTIFIER:
        {
            Identifier_t id = getIdFromTable(&context->nameTable, node->value.id);
            if (id.type == FUNC_ID) fillColor = FUNC_ID_COLOR;
            if (id.type == VAR_ID) fillColor = VAR_ID_COLOR;

            fprintf(dotFile, "TYPE = ID(%d) | %s(%d) | ", node->type, id.str, node->value.id);
            break;
        }
        case NUMBER:
            fillColor = NUMBER_COLOR;
            fprintf(dotFile, "TYPE = NUM(%d) | %lg | ", node->type, node->value.number);
            break;
        default:
            fprintf(dotFile, "TYPE = ???(%d) | %d | ", node->type, node->value.id);
            break;
    }

    if (!minified)
        fprintf(dotFile, " { <left>left[%p] | <right>right[%p] }}\"", node->left, node->right);
    else {
        fprintf(dotFile, " {<left> L | <right> R}}\"");
    }

    fprintf(dotFile, ", style = filled, fillcolor = \"%s\"];\n", fillColor);

    if (node->left) {
        fprintf(dotFile, "\tnode%p:<left> -> node%p;\n", node, node->left);
        recursiveDumpTree(context, node->left, minified, dotFile);
    }

    if (node->right) {
        fprintf(dotFile, "\tnode%p:<right> -> node%p;\n", node, node->right);
        recursiveDumpTree(context, node->right, minified, dotFile);
    }

    return true;
}

bool deleteTree(Node_t *node) {
    if (!node) return true;

    logPrint(L_EXTRA, 0, "ExprTree:Deleting tree[%p]\n", node);

    if (node->left)
        deleteTree(node->left);

    if (node->right)
        deleteTree(node->right);

    free(node);

    return true;
}


/*========================Writing to intermediate representation================================================*/
static void printTabulation(FILE* file, unsigned tabs) {
    for (int i = 0; i < tabs; i++)
        putc('\t', file);
}

static const char *getASTString(size_t id) {
    for (size_t idx = 0; idx < ARRAY_SIZE(ASTNames); idx++) {
        if (ASTNames[idx].opCode == id)
            return ASTNames[idx].name;
    }
    return NULL;
}

static int getOpCode(const char *ASTString) {
    for (size_t idx = 0; idx < ARRAY_SIZE(ASTNames); idx++) {
        if (strcmp(ASTNames[idx].name, ASTString) == 0)
            return ASTNames[idx].opCode;
    }

    return -1;
}


ASTStatus_t writeTreeToAST(Node_t *node, FILE *file, unsigned tabulation) {

    printTabulation(file, tabulation);
    fprintf(file, "{");

    if (node) {
        switch(node->type) {
            case NUMBER:
                fprintf(file, "NUM:%lg", node->value.number);
                break;
            case IDENTIFIER:
                fprintf(file, "IDR:%d", node->value.id);
                break;
            case OPERATOR:
            {
                const char *ASTString = getASTString(node->value.op);
                fprintf(file, "OPR:%s\n", ASTString);

                writeTreeToAST(node->left,  file, tabulation + 1);
                writeTreeToAST(node->right, file, tabulation + 1);

                printTabulation(file, tabulation);
                break;
            }
            default:
                assert(0);
                break;
        }
    }

    fprintf(file, "}\n");

    return AST_SUCCESS;
}


ASTStatus_t writeAsAST(LangContext_t *context) {
    assert(context);
    assert(context->tree);
    assert(context->outputFileName);

    FILE *file = fopen(context->outputFileName, "w");
    if (!file) {
        logPrint(L_ZERO, 1, "Can't open file '%s' for writing\n", context->outputFileName);
        return AST_FILE_ERROR;
    }

    fprintf(file, AST_SIGNATURE_STRING "%d\n", AST_FORMAT_VERSION);

    NameTableWrite(&context->nameTable, file);
    fprintf(file, "\n");
    writeTreeToAST(context->tree, file, 0);
    fprintf(file, "\n");
    fclose(file);

    return AST_SUCCESS;
}

/*================PREFIX TREE FORMAT PARSING==============================*/
static bool readSignature(LangContext_t *context, const char **text) {
    int shift = 0;
    int ASTVersion = 0;
    if (sscanf(*text, AST_SIGNATURE_STRING "%d%n", &ASTVersion, &shift) != 1) {
        logPrint(L_ZERO, 1, "Wrong signature format\n");
        return false;
    }

    if (ASTVersion != AST_FORMAT_VERSION) {
        logPrint(L_ZERO, 1, "Incorrect format version: expected %d, got %d\n", AST_FORMAT_VERSION, ASTVersion);
        return false;
    }

    *text += shift;

    return true;
}

ASTStatus_t readFromAST(LangContext_t *context) {
    const char *text = context->text;

    if (!readSignature(context, &text))
        return AST_SIGNATURE_ERROR;

    if (NameTableRead(&context->nameTable, &text) != NAMETABLE_SUCCESS)
        return AST_SYNTAX_ERROR;

    Node_t *tree = readTreeFromAST(context, NULL, &text);
    if (!tree)
        return AST_SYNTAX_ERROR;

    context->tree = tree;
    return AST_SUCCESS;
}

static enum ElemType getNodeType(const char *nodeTypeStr) {
    logPrint(L_EXTRA, 0, "AST: node type %s\n", nodeTypeStr);
    if (strcmp(nodeTypeStr, "OPR") == 0)
        return OPERATOR;
    else if (strcmp(nodeTypeStr, "NUM") == 0)
        return NUMBER;
    else if (strcmp(nodeTypeStr, "IDR") == 0)
        return IDENTIFIER;
    else {
        logPrint(L_ZERO, 1, "Unknown node type: %s\n", nodeTypeStr);
        return UNKNOWN_TYPE;
    }
}

#define MOVE_TEXT do {*text += shift; shift = 0;} while(0)
Node_t *readTreeFromAST(LangContext_t *context, Node_t *parent, const char **text) {
    // if (context->mode != FRONTEND_BACKWARD) {
    //     logPrint(L_ZERO, 1, "Wrong frontend mode\n");
    //     return NULL;
    // }

    int shift = 0;
    sscanf(*text, " { }%n", &shift);
    if (shift != 0) {
        MOVE_TEXT;
        return NULL;
    }

    char buffer[AST_BUFFER_SIZE] = "";
    sscanf(*text, " { %[^:]:%n", buffer, &shift);
    if (shift == 0) {
        logPrint(L_ZERO, 1, "Wrong format: expected { at '%.15s'\n", *text);
        return NULL;
    }

    MOVE_TEXT;

    Node_t *node = getEmptyNode(context);
    if (!node) {
        logPrint(L_EXTRA, 1, "Can't allocate node in readTreeFromAST\n");
        return NULL;
    }

    node->type = getNodeType(buffer);
    node->parent = parent;

    switch (node->type) {
        case NUMBER:
            if (sscanf(*text, " %lg%n", &node->value.number, &shift) != 1) {
                logPrint(L_ZERO, 1, "PREFIX_READ: Can't read number\n");
                return NULL;
            }
            MOVE_TEXT;
            break;

        case IDENTIFIER:
            if (sscanf(*text, " %d%n", &node->value.id, &shift) != 1) {
                logPrint(L_ZERO, 1, "PREFIX_READ: Can't read identifier\n");
                return NULL;
            }
            MOVE_TEXT;
            break;

        case OPERATOR:
        {
            if (sscanf(*text, " %[^ \n\t{}]%n", buffer, &shift) != 1) {
                logPrint(L_ZERO, 1, "PREFIX_READ: Can't read operator\n");
                return NULL;
            }
            MOVE_TEXT;

            int opCode = getOpCode(buffer);
            if (opCode == -1) {
                logPrint(L_ZERO, 1, "Unknown operator '%s'\n", buffer);
                return NULL;
            }
            node->value.op = (enum OperatorType) opCode;

            logPrint(L_EXTRA, 0, "Scanning left subtree: '%.20s'\n", *text);
            node->left  = readTreeFromAST(context, node, text);

            logPrint(L_EXTRA, 0, "Scanning right subtree: '%.20s'\n", *text);
            node->right = readTreeFromAST(context, node, text);

            break;
        }
        case UNKNOWN_TYPE:
        default:
            logPrint(L_ZERO, 1, "Something went wrong\n");
            assert(0);
    }

    sscanf(*text, " }%n", &shift);
    if (shift == 0) {
        logPrint(L_ZERO, 1, "Wrong format: no } at '%s'\n", *text);
        return NULL;
    }
    MOVE_TEXT;

    return node;
}
#undef MOVE_TEXT
