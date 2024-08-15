int init(){
    int sock;
    fil=open("/443/pri/user.txt",O_RDWR|O_CREAT);
    int flog=open("/443/pri/log.dat",O_RDWR|O_APPEND|O_CREAT);
    mylog=(char*)mmap(0,100*1024,PROT_READ|PROT_WRITE,MAP_SHARED,flog,0);
    user=(char(*)[128])mmap(0,0x5AA5D000,PROT_READ|PROT_WRITE,MAP_SHARED,fil,0);
    for(int i=0;*user[i];i++)users[(std::string)user[i]]=i;
    ldata=lseek(fdata=open("/443/pri/data.dat",O_RDWR|O_APPEND|O_CREAT),0,SEEK_END);
    data=(char*)mmap(0,4ll<<30,PROT_READ|PROT_WRITE,MAP_SHARED,fdata,0);
    lcont=lseek(fcont=open("/443/pri/cont.dat",O_RDWR|O_APPEND|O_CREAT),0,SEEK_END);
    cont=(char*)mmap(0,20ll<<30,PROT_READ|PROT_WRITE,MAP_SHARED,fcont,0);
    printf("user:%d\ndata:%lld\ncontent:%lld\n",(int)users.size(),ldata,lcont);
	e.push_back((point){"POST /api/login",login});
	e.push_back((point){"POST /api/register",reg});
	e.push_back((point){"GET /api/user",check_cookie_js});
	e.push_back((point){"POST /api/change_password",change_password});
	e.push_back((point){"GET /p=",getp});
	e.push_back((point){"GET /con=",getcon});
	e.push_back((point){"POST /api/sendmessage",postmsg});
	e.push_back((point){"GET /logout",logout});
	e.push_back((point){"GET /api_admin",api_admin});
    srand(time(0));
    struct sockaddr_in addr;
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
    ctx = SSL_CTX_new(SSLv23_server_method());
    if (!ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    if(SSL_CTX_use_certificate_chain_file(ctx, "/443/pri/free.neuqboard.cn.crt")<=0){
        printf("ERROR crt\n");
    }
    if(SSL_CTX_use_PrivateKey_file(ctx, "/443/pri/free.neuqboard.cn.key", SSL_FILETYPE_PEM)<=0){
        printf("ERROR KEY\n");
    }
    if (!SSL_CTX_check_private_key(ctx)) {
        printf("Private key does not match the public certificate\n");
    }
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Unable to create socket");
        exit(EXIT_FAILURE);
    }
    addr.sin_family = AF_INET;
    addr.sin_port = htons(999);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Unable to bind");
        exit(EXIT_FAILURE);
    }
    if (listen(sock, 1) < 0) {
        perror("Unable to listen");
        exit(EXIT_FAILURE);
    }
    return sock;
}