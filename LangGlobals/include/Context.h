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
    OP_SUB,        ///< -
    OP_MUL,        ///< *
    OP_DIV,        ///< /
    OP_POW,        ///< ^
    OP_SIN,        ///< sin
    OP_COS,        ///< cos
    OP_SINH,       ///< sh
    OP_COSH,       ///< ch
    OP_TAN,        ///< tan
    OP_CTG,        ///< ctg
    OP_LOGN,       ///< ln
//control flow operators
    OP_ASSIGN,     ///< =
    // OP_IF,         ///< if
    // OP_WHILE,      ///< while
    // OP_FUNC_DECL,  ///< declare function
    // OP_VAR_DECL,   ///< declare variable
    OP_IN,         ///< scanf, cin
    OP_OUT,        ///< printf, cout
//other operators
    OP_LBRACKET,   ///< (
    OP_RBRACKET,   ///< )
    OP_SEMICOLON,  ///< ;
    OP_TO,         ///< Just keyword, does nothing

    OP_DOLLAR,     /// $
    OP_RUBLE,      /// ₽
    OP_EOF         ///< \0
};

typedef struct {
    enum OperatorType opCode;
    bool binary;            ///< Has two arguments
    const char *str;        ///< string to write in dump and search in language
    int priority;      ///< Priority of operation (bigger = executes first) (affects only tex)
} Operator_t;

const Operator_t operators[] = {
    {.opCode = OP_ADD,  .binary = 1, .str = "+"   , .priority = 0},
    {.opCode = OP_SUB,  .binary = 1, .str = "-"   , .priority = 0},
    {.opCode = OP_MUL,  .binary = 1, .str = "*"   , .priority = 1},
    {.opCode = OP_DIV,  .binary = 1, .str = "/"   , .priority = 1},
    {.opCode = OP_POW,  .binary = 1, .str = "^"   , .priority = 2},
    {.opCode = OP_SIN,  .binary = 0, .str = "sin" , .priority = 3},
    {.opCode = OP_COS,  .binary = 0, .str = "cos" , .priority = 3},
    {.opCode = OP_SINH, .binary = 0, .str = "sh"  , .priority = 3},
    {.opCode = OP_COSH, .binary = 0, .str = "ch"  , .priority = 3},
    {.opCode = OP_TAN,  .binary = 0, .str = "tg"  , .priority = 3},
    {.opCode = OP_CTG,  .binary = 0, .str = "ctg" , .priority = 3},
    {.opCode = OP_LOGN, .binary = 0, .str = "ln"  , .priority = 3},

    {.opCode = OP_ASSIGN,       .binary = 2, .str = "="           , .priority = -1},
    // {.opCode = OP_IF,           .binary = 0, .str = "if"            , .priority = 3},
    // {.opCode = OP_WHILE,        .binary = 0, .str = "while"         , .priority = 3},
    // {.opCode = OP_FUNC_DECL,    .binary = 0, .str = "Transaction"   , .priority = 3},
    // {.opCode = OP_VAR_DECL,     .binary = 0, .str = "Account"       , .priority = 3},
    {.opCode = OP_IN,           .binary = 0, .str = "Invest"        , .priority = 3},
    {.opCode = OP_OUT,          .binary = 0, .str = "ShowBalance"   , .priority = 3},

    {.opCode = OP_LBRACKET,    .binary = 0, .str = "("  , .priority = 3},
    {.opCode = OP_RBRACKET,    .binary = 0, .str = ")"  , .priority = 3},
    {.opCode = OP_SEMICOLON,   .binary = 2, .str = "%"  , .priority = -2},
    {.opCode = OP_TO,          .binary = 0, .str = "to" , .priority = 3},

    {.opCode = OP_DOLLAR,      .binary = 0, .str = "$" , .priority = 10},
    {.opCode = OP_RUBLE,       .binary = 0, .str = "₽" , .priority = 10},

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
