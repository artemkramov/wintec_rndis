/**
* Redefined system function due to Android features
*/
extern int systemCall(const char *command);

/**
* Redefined popen function due to Android features
*/
extern FILE* popenCall(char *program, register char *type);