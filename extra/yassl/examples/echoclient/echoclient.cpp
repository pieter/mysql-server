/* echoclient.cpp  */

#include "../../testsuite/test.hpp"


void echoclient_test(void* args)
{
#ifdef _WIN32
    WSADATA wsd;
    WSAStartup(0x0002, &wsd);
#endif

    SOCKET_T sockfd = 0;
    int      argc = 0;
    char**   argv = 0;

    FILE* fin  = stdin;
    FILE* fout = stdout;

    bool inCreated  = false;
    bool outCreated = false;

    set_args(argc, argv, *static_cast<func_args*>(args));
    if (argc >= 2) {
        fin  = fopen(argv[1], "r"); 
        inCreated = true;
    }
    if (argc >= 3) {
        fout = fopen(argv[2], "w");
        outCreated = true;
    }

    if (!fin)  err_sys("can't open input file");
    if (!fout) err_sys("can't open output file");

    tcp_connect(sockfd);

    SSL_METHOD* method = TLSv1_client_method();
    SSL_CTX*    ctx = SSL_CTX_new(method);
    set_certs(ctx);
    SSL*        ssl = SSL_new(ctx);

    SSL_set_fd(ssl, sockfd);
    if (SSL_connect(ssl) != SSL_SUCCESS) err_sys("SSL_connect failed");

    char send[1024];
    char reply[1024];
 
    while (fgets(send, sizeof(send), fin)) {

        int sendSz = strlen(send) + 1;
        if (SSL_write(ssl, send, sendSz) != sendSz)
            err_sys("SSL_write failed");

        if (strncmp(send, "quit", 4) == 0) {
            fputs("sending server shutdown command: quit!\n", fout);
            break;
        }

        if (SSL_read(ssl, reply, sizeof(reply)) > 0) 
            fputs(reply, fout);
    }

    SSL_CTX_free(ctx);
    SSL_free(ssl);

    fflush(fout);
    if (inCreated)  fclose(fin);
    if (outCreated) fclose(fout);

    ((func_args*)args)->return_code = 0;
}


#ifndef NO_MAIN_DRIVER

    int main(int argc, char** argv)
    {
        func_args args;

        args.argc = argc;
        args.argv = argv;

        echoclient_test(&args);

        return args.return_code;
    }

#endif // NO_MAIN_DRIVER
