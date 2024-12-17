#ifndef CONTEXT_H
#define CONTEXT_H

#define DOTS_DIR "dot"
#define IMGS_DIR "img"

#define EXPR_TREE_DUMP_DOT_FORMAT "expr_%04zu.dot"
#define EXPR_TREE_DUMP_IMG_FORMAT "expr_dump_%04zu."

const char * const OPERATOR_COLOR       = "#EDE1CF";
const char * const IDENTIFIER_COLOR     = "#f3d4de";
const char * const NUMBER_COLOR         = "#107980";
const char * const DEFAULT_NODE_COLOR   = "#000000";
const size_t DUMP_BUFFER_SIZE = 128;

#define IR_SIGNATURE_STRING "IR:"
const int IR_FORMAT_VERSION = 1;

enum ElemType {
    OPERATOR,
    IDENTIFIER,
    NUMBER,
    UNKNOWN_TYPE
};

enum OperatorType {
//mathematical operators
    OP_ADD = 0,    ///< +
    OP_SUB = 1,        ///< -
    OP_MUL = 2,        ///< *
    OP_DIV = 3,        ///< /
    OP_POW = 4,        ///< ^
    OP_SQRT = 5,       ///< sqrt()
    OP_SIN = 6,        ///< sin
    OP_COS = 7,        ///< cos
    OP_SINH = 8,       ///< sh
    OP_COSH = 9,       ///< ch
    OP_TAN = 10,        ///< tan
    OP_CTG = 11,        ///< ctg
    OP_LOGN = 12,       ///< ln
//control flow operators
    OP_ASSIGN = 13,     ///< =
    OP_IF = 14,         ///< if
    OP_WHILE = 15,      ///< while
    OP_FUNC_DECL = 16,  ///< declare function
    // OP_VAR_DECL,   ///< declare variable
    OP_IN = 17,         ///< scanf, cin
    OP_OUT = 18,        ///< printf, cout
//other operators
    OP_LBRACKET = 19,   ///< (
    OP_RBRACKET = 20,   ///< )
    OP_LABRACKET = 21,  ///< <
    OP_RABRACKET = 22,  ///< >
    OP_SEMICOLON = 23,  ///< ;
    OP_ARROW = 24,      ///< ->

    OP_COMMA = 25,      /// ,

    OP_DOLLAR = 26,     /// $
    OP_RUBLE = 27,      /// ₽

    OP_RET  = 28,
    OP_CALL = 29,       ///
    OP_FUNC_HEADER = 30,
    OP_EOF = 31        ///< \0
};

typedef struct {
    enum OperatorType opCode;
    bool binary;            ///< Has two arguments
    const char *str;        ///< string to search in language
    const char *dotStr;     ///< string to write in dump
    const char *asmStr;     ///< ~analog in asm code
    int priority;      ///< Priority of operation (bigger = executes first) (affects only tex)
} Operator_t;

const Operator_t operators[] = {
    {.opCode = OP_ADD,  .binary = 1, .str = "+"   , .dotStr = "+",    .asmStr = "ADD", .priority = 0},
    {.opCode = OP_SUB,  .binary = 1, .str = "-"   , .dotStr = "-",    .asmStr = "SUB", .priority = 0},
    {.opCode = OP_MUL,  .binary = 1, .str = "*"   , .dotStr = "*",    .asmStr = "MUL", .priority = 1},
    {.opCode = OP_DIV,  .binary = 1, .str = "/"   , .dotStr = "/",    .asmStr = "DIV", .priority = 1},
    {.opCode = OP_POW,  .binary = 1, .str = "^"   , .dotStr = "^",    .asmStr = "", .priority = 2},
    {.opCode = OP_SQRT, .binary = 0, .str = "sqrt", .dotStr = "sqrt", .asmStr = "SQRT", .priority = 3},
    {.opCode = OP_SIN,  .binary = 0, .str = "sin" , .dotStr = "sin",  .asmStr = "SIN", .priority = 3},
    {.opCode = OP_COS,  .binary = 0, .str = "cos" , .dotStr = "cos",  .asmStr = "COS", .priority = 3},
    {.opCode = OP_SINH, .binary = 0, .str = "sh"  , .dotStr = "sh",   .asmStr = "", .priority = 3},
    {.opCode = OP_COSH, .binary = 0, .str = "ch"  , .dotStr = "ch",   .asmStr = "", .priority = 3},
    {.opCode = OP_TAN,  .binary = 0, .str = "tg"  , .dotStr = "tg",   .asmStr = "", .priority = 3},
    {.opCode = OP_CTG,  .binary = 0, .str = "ctg" , .dotStr = "ctg",  .asmStr = "", .priority = 3},
    {.opCode = OP_LOGN, .binary = 0, .str = "ln"  , .dotStr = "ln",   .asmStr = "", .priority = 3},

    {.opCode = OP_ASSIGN,    .binary = 2, .str = "=", .dotStr = "=", .priority = -1},
    {.opCode = OP_IF,        .binary = 0, .str = "if", .dotStr = "if", .priority = -2},
    {.opCode = OP_WHILE,     .binary = 0, .str = "while", .dotStr = "while", .priority = -2},
    {.opCode = OP_FUNC_DECL, .binary = 0, .str = "Transaction", .dotStr = "Transaction", .priority = -2},
    // {.opCode = OP_VAR_DECL,     .binary = 0, .str = "Account"       , .priority = 3},
    {.opCode = OP_IN,       .binary = 0, .str = "Invest",      .dotStr = "In",  .asmStr = "IN",  .priority = 3},
    {.opCode = OP_OUT,      .binary = 0, .str = "ShowBalance", .dotStr = "Out", .asmStr = "OUT", .priority = 3},

    {.opCode = OP_LBRACKET,    .binary = 0, .str = "("  , .dotStr = "(", .priority = 3},
    {.opCode = OP_RBRACKET,    .binary = 0, .str = ")"  , .dotStr = ")", .priority = 3},
    {.opCode = OP_LABRACKET,   .binary = 1, .str = "<"  , .dotStr = "&lt", .asmStr = "CALL __LESS\n", .priority = -1},
    {.opCode = OP_RABRACKET,   .binary = 1, .str = ">"  , .dotStr = "&gt", .asmStr = "CALL __GREATER\n", .priority = -1},
    {.opCode = OP_SEMICOLON,   .binary = 1, .str = "%"  , .dotStr = ";", .priority = -2},
    {.opCode = OP_ARROW,       .binary = 1, .str = "->" , .dotStr = "->", .priority = 3},

    {.opCode = OP_COMMA,       .binary = 1, .str = "," , .dotStr = ",",   .priority = -2},

    {.opCode = OP_DOLLAR,      .binary = 0, .str = "$"},
    {.opCode = OP_RUBLE,       .binary = 0, .str = "₽"},

    {.opCode = OP_RET,         .binary = 0, .str = "Pay", .dotStr = "ret"},
    {.opCode = OP_CALL,        .binary = 1, .str = "", .dotStr = "call", .priority = -2},
    {.opCode = OP_FUNC_HEADER, .binary = 1, .str = "", .dotStr = "function header"},
    {.opCode = OP_EOF,         .binary = 0, .str = "__EOF__" , .priority = -1}
};

union NodeValue {
    double number;        ///< Floating point number
    enum OperatorType op; ///< +, -, /, etc.
    int id;               ///< x, y, z, etc.
};

typedef struct Node_t {
    Node_t *parent;

    enum ElemType type;

    union NodeValue value;

    Node_t *left;
    Node_t *right;
} Node_t;


typedef struct Token_t {
    Node_t node;

    size_t line;        ///< Position of token in code
    size_t column;
} Token_t;


typedef struct {
    const char *inputFileName;
    const char *outputFileName;
    const char *text;

    NameTable_t nameTable;

    MemoryArena_t treeMemory;
    Node_t *tree;

    int mode; /// 0 frontend 1 inverse frontend
} LangContext_t;

typedef enum IRStatus_t {
    IR_SUCCESS,
    IR_MEMORY_ERROR,
    IR_FILE_ERROR,
    IR_SIGNATURE_ERROR,
    IR_SYNTAX_ERROR
} IRStatus_t;

/*==========================Old functions for tree========================*/
Node_t *createNode(enum ElemType type, int iVal, double dVal, Node_t *left, Node_t *right);

Node_t *copyTree(Node_t *node);

bool deleteTree(Node_t *node);
/*============================New function for creating tree==============*/

//!Warning: gives node from MemoryArena, so free is not supported
//!All tree will be deleted using freeMemoryArena
Node_t *getEmptyNode(LangContext_t *context);


/*============================IR reading and writing======================================*/

IRStatus_t readFromIR(LangContext_t *context);
IRStatus_t writeAsIR(LangContext_t *context);

Node_t *readTreeFromIR(LangContext_t *context, Node_t *parent, const char **text);
IRStatus_t writeTreeToIR(Node_t *node, FILE *file, unsigned tabulation);

/*==================================================================*/
/// @brief Dump tree using graphviz
/// @param context FrontendAlgebra context
/// @param node Root node of the tree
/// @param minified Do not show pointers if true
bool dumpTree(LangContext_t *context, Node_t *node, bool minified);

#if defined(_TREE_DUMP) && !defined(NDEBUG)

#define DUMP_TREE(context, node, minified) \
    logPrint(L_ZERO, 0, "<h2>Node_t dump called from %s:%d %s</h2>\n", __FILE__, __LINE__, __PRETTY_FUNCTION__); \
    dumpTree(context, node, minified);

#else

# define DUMP_TREE(...)

#endif
#endif
