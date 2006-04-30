/* client.cpp  */

#include "../../testsuite/test.hpp"

//#define TEST_RESUME


void client_test(void* args)
{
#ifdef _WIN32
    WSADATA wsd;
    WSAStartup(0x0002, &wsd);
#endif

    SOCKET_T sockfd = 0;
    int      argc = 0;
    char**   argv = 0;

    set_args(argc, argv, *static_cast<func_args*>(args));
    tcp_connect(sockfd);

    SSL_METHOD* method = TLSv1_client_method();
    SSL_CTX*    ctx = SSL_CTX_new(method);

    set_certs(ctx);
    SSL* ssl = SSL_new(ctx);

    SSL_set_fd(ssl, sockfd);

    if (SSL_connect(ssl) != SSL_SUCCESS) err_sys("SSL_connect failed");
    showPeer(ssl);

    const char* cipher = 0;
    int index = 0;
    char list[1024];
    strcpy(list, "cipherlist");
    while ( (cipher = SSL_get_cipher_list(ssl, index++)) ) {
        strcat(list, ":");
        strcat(list, cipher);
    }
    printf("%s\n", list);
    printf("Using Cipher Suite %s\n", SSL_get_cipher(ssl));

    char msg[] = "hello yassl!";
    if (SSL_write(ssl, msg, sizeof(msg)) != sizeof(msg))
        err_sys("SSL_write failed");

    char reply[1024];
    reply[SSL_read(ssl, reply, sizeof(reply))] = 0;
    printf("Server response: %s\n", reply);

#ifdef TEST_RESUME
    SSL_SESSION* session   = SSL_get_session(ssl);
    SSL*         sslResume = SSL_new(ctx);
#endif

    SSL_shutdown(ssl);
    SSL_free(ssl);

#ifdef TEST_RESUME
    tcp_connect(sockfd);
    SSL_set_fd(sslResume, sockfd);
    SSL_set_session(sslResume, session);
    
    if (SSL_connect(sslResume) != SSL_SUCCESS) err_sys("SSL resume failed");
  
    if (SSL_write(sslResume, msg, sizeof(msg)) != sizeof(msg))
        err_sys("SSL_write failed");

    reply[SSL_read(sslResume, reply, sizeof(reply))] = 0;
    printf("Server response: %s\n", reply);

    SSL_shutdown(sslResume);
    SSL_free(sslResume);
#endif // TEST_RESUME

    SSL_CTX_free(ctx);
    ((func_args*)args)->return_code = 0;
}


#ifndef NO_MAIN_DRIVER

    int main(int argc, char** argv)
    {
        func_args args;

        args.argc = argc;
        args.argv = argv;

        client_test(&args);
        return args.return_code;
    }

#endif // NO_MAIN_DRIVER

