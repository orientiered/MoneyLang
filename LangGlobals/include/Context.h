#ifndef CONTEXT_H
#define CONTEXT_H

#define DOTS_DIR "dot"
#define IMGS_DIR "img"

#define EXPR_TREE_DUMP_DOT_FORMAT "expr_%04zu.dot"
#define EXPR_TREE_DUMP_IMG_FORMAT "expr_dump_%04zu."

const char * const OPERATOR_COLOR       = "#EDE1CF";
const char * const SEP_COLOR            = "#CCCCCC";
const char * const VAR_ID_COLOR         = "#f3d4de";
const char * const FUNC_ID_COLOR        = "#24e658";
const char * const NUMBER_COLOR         = "#107980";
const char * const DEFAULT_NODE_COLOR   = "#000000";
const size_t DUMP_BUFFER_SIZE = 128;
const size_t IR_BUFFER_SIZE = 64;
#define IR_SIGNATURE_STRING "IR312:"
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
    OP_SQRT,       ///< sqrt()
    OP_SIN,        ///< sin
    OP_COS,        ///< cos
    OP_SINH,       ///< sh
    OP_COSH,       ///< ch
    OP_TAN,        ///< tan
    OP_CTG,        ///< ctg
    OP_LOGN,       ///< ln
//control flow operators
    OP_ASSIGN,     ///< =
    OP_IF,         ///< if
    OP_ELSE,       ///< else
    OP_WHILE,      ///< while
    OP_FUNC_DECL,  ///< declare function
    OP_VAR_DECL,   ///< declare variable
    OP_IN,         ///< scanf, cin
    OP_OUT,        ///< printf, cout
    OP_TEXT,       ///< print constant string
//other operators
    OP_LBRACKET,   ///< (
    OP_RBRACKET,   ///< )
    OP_LABRACKET,  ///< <
    OP_RABRACKET,  ///< >
    OP_GREAT_EQ,   ///< >=
    OP_LESS_EQ,    ///< <=
    OP_EQUAL,      ///< ==
    OP_NEQUAL,     ///< !=
    OP_SEP,        ///< ;
    OP_ARROW,      ///< ->
    OP_QUOTE,      ///< "
    OP_RET,        ///< pay

    OP_COMMA,      /// ,
//language specific tokens
    OP_DOLLAR,     /// $
    OP_RUBLE,      /// ₽

    OP_CALL,
    OP_FUNC_HEADER,
    OP_EOF        ///< \0
};

typedef struct {
    enum OperatorType opCode;
    bool binary;            ///< Has two arguments
    bool isFunction;        ///< sin(), cos(), ....
    const char *str;        ///< string to search in language
    const char *dotStr;     ///< string to write in dump. If not specified, str is used
    const char *asmStr;     ///< ~analog in asm code
    int priority;           ///< Priority of operation (bigger = executes first) (affects only brackets)
} Operator_t;

//! all operators must be in the same order as in enum
const Operator_t operators[] = {
    {.opCode = OP_ADD,  .binary = 1, .str = "+"   , .asmStr = "ADD", .priority = 0},
    {.opCode = OP_SUB,  .binary = 1, .str = "-"   , .asmStr = "SUB", .priority = 0},
    {.opCode = OP_MUL,  .binary = 1, .str = "*"   , .asmStr = "MUL", .priority = 1},
    {.opCode = OP_DIV,  .binary = 1, .str = "/"   , .asmStr = "DIV", .priority = 1},
    {.opCode = OP_POW,  .binary = 1, .str = "^"   , .asmStr = "", .priority = 2},
    {.opCode = OP_SQRT, .binary = 0, .isFunction = 1, .str = "sqrt", .asmStr = "SQRT", .priority = 3},
    {.opCode = OP_SIN,  .binary = 0, .isFunction = 1, .str = "sin" , .asmStr = "SIN", .priority = 3},
    {.opCode = OP_COS,  .binary = 0, .isFunction = 1, .str = "cos" , .asmStr = "COS", .priority = 3},
    {.opCode = OP_SINH, .binary = 0, .isFunction = 1, .str = "sh"  , .asmStr = "", .priority = 3}, //TODO: remove
    {.opCode = OP_COSH, .binary = 0, .isFunction = 1, .str = "ch"  , .asmStr = "", .priority = 3}, //TODO: remove
    {.opCode = OP_TAN,  .binary = 0, .isFunction = 1, .str = "tg"  , .asmStr = "", .priority = 3}, //TODO: remove
    {.opCode = OP_CTG,  .binary = 0, .isFunction = 1, .str = "ctg" , .asmStr = "", .priority = 3}, //TODO: remove
    {.opCode = OP_LOGN, .binary = 0, .isFunction = 1, .str = "ln"  , .asmStr = "", .priority = 3}, //TODO: remove

    {.opCode = OP_ASSIGN,    .binary = 1, .str = "=",     .priority = -1},
    {.opCode = OP_IF,        .binary = 0, .str = "if",    .priority = -2},
    {.opCode = OP_ELSE,      .binary = 0, .str = "else",  .priority = -2},
    {.opCode = OP_WHILE,     .binary = 0, .str = "while", .priority = -2},
    {.opCode = OP_FUNC_DECL, .binary = 0, .str = "Transaction", .dotStr = "Function decl", .priority = -2},
    {.opCode = OP_VAR_DECL,  .binary = 0, .str = "Account"    , .dotStr = "Variable decl",  .priority = -2},
    {.opCode = OP_IN,        .binary = 0, .str = "Invest",      .dotStr = "In",  .asmStr = "IN",  .priority = 3},
    {.opCode = OP_OUT,       .binary = 0, .str = "ShowBalance", .dotStr = "Out", .asmStr = "OUT", .priority = 3},
    {.opCode = OP_TEXT,      .binary = 0, .str = "Txt", .dotStr = "Text", .asmStr = "CALL __STR_PRINT", .priority = 3},

    {.opCode = OP_LBRACKET  , .binary = 0, .str = "("  , .priority = 3},
    {.opCode = OP_RBRACKET  , .binary = 0, .str = ")"  , .priority = 3},
    {.opCode = OP_LABRACKET , .binary = 1, .str = "<"  , .dotStr = "less"   , .asmStr = "CALL __LESS\n"      , .priority = -1},
    {.opCode = OP_RABRACKET , .binary = 1, .str = ">"  , .dotStr = "greater", .asmStr = "CALL __GREATER\n"   , .priority = -1},
    {.opCode = OP_GREAT_EQ  , .binary = 1, .str = ">==", .dotStr = "geq"    , .asmStr = "CALL __GREATER_EQ\n", .priority = -1},
    {.opCode = OP_LESS_EQ   , .binary = 1, .str = "==<", .dotStr = "leq"    , .asmStr = "CALL __LESS_EQ\n"   , .priority = -1},
    {.opCode = OP_EQUAL     , .binary = 1, .str = "===", .dotStr = "=="     , .asmStr = "CALL __EQUAL\n"     , .priority = -1},
    {.opCode = OP_NEQUAL    , .binary = 1, .str = "!==", .dotStr = "!="     , .asmStr = "CALL __NEQUAL\n"    , .priority = -1},
    {.opCode = OP_SEP       , .binary = 1, .str = "%"  , .dotStr = ";"      , .priority = -2},
    {.opCode = OP_ARROW     , .binary = 1, .str = "->" , .priority = 3},
    {.opCode = OP_QUOTE     , .binary = 0, .str = "\"" , .priority = 3},
    {.opCode = OP_RET       , .binary = 0, .str = "Pay", .dotStr = "ret"},

    {.opCode = OP_COMMA     , .binary = 1, .str = ","  , .priority = -2},

    {.opCode = OP_DOLLAR    , .binary = 0, .str = "$"},
    {.opCode = OP_RUBLE     , .binary = 0, .str = "₽"},

    {.opCode = OP_CALL,        .binary = 1, .str = NULL, .dotStr = "call", .priority = -2},
    {.opCode = OP_FUNC_HEADER, .binary = 1, .str = NULL, .dotStr = "function header"},
    {.opCode = OP_EOF,         .binary = 0, .str = NULL, .dotStr = "__EOF__" , .priority = -1}
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

typedef struct IRName_t {
    enum OperatorType opCode;
    const char *name;
} IRName_t;

const IRName_t IRNames[] = {
    {OP_ADD , "ADD" },
    {OP_SUB , "SUB" },
    {OP_MUL , "MUL" },
    {OP_DIV , "DIV" },

    {OP_POW , "POW" },
    {OP_SQRT, "SQRT"},
    {OP_SIN , "SIN" },
    {OP_COS , "COS" },
    {OP_TAN , "TAN" },

    {OP_LABRACKET, "LESS" },
    {OP_RABRACKET, "GREATER" },
    {OP_GREAT_EQ,  "GREATER_EQ"},
    {OP_LESS_EQ,   "LESS_EQ"},
    {OP_EQUAL,     "EQUAL"},
    {OP_NEQUAL,    "N_EQUAL"},

    {OP_IN,        "IN"},
    {OP_OUT,       "OUT"},
    {OP_TEXT,      "TEXT"},
    {OP_ASSIGN,    "ASSIGN"},
    {OP_IF,        "IF" },
    {OP_ELSE,      "ELSE"},
    {OP_WHILE,     "WHILE"},

    {OP_SEP,       "SEP"},
    {OP_COMMA,     "ARG_SEP"},

    {OP_VAR_DECL,  "VAR"},
    {OP_FUNC_DECL, "DEF"},
    {OP_CALL,      "CALL"},
    {OP_RET,       "RET"},
    {OP_FUNC_HEADER, "FUNC_HDR"}
};

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
