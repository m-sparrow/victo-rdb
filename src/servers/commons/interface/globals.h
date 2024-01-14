typedef struct {
    char* dbBasePath;
    char* ipAddress;
    int port;
    int maxStackSize;
} WebsocketParams;

void setWebSocketParams(WebsocketParams params);
char* getDatabasePath();
char* getWebsockInitIP();
int getWebsockInitPort();
void cleanupWebSocketParams();