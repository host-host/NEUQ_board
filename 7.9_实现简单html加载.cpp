#include<windows.h>
#include<bits/stdc++.h>
using namespace std;
struct CInitSock {
	CInitSock(BYTE minorVer=2,BYTE majorVer=2) {
		WSADATA wsaData;
		WORD VersionRequset;
		VersionRequset=MAKEWORD(minorVer,majorVer);
		if(WSAStartup(VersionRequset,&wsaData)!=0)exit(-1);
	}
} Tmp_sock;
char* GetIp() {
	char szText[256];
	int iRet=gethostname(szText,256);
	if(iRet!=0) {
		printf("Gethostname Failed!");
		exit(-1);
	}
	HOSTENT *host=gethostbyname(szText);
	if(NULL==host) {
		printf("Gethostbyname Failed!");
		exit(-1);
	}
	in_addr PcAddr;
	for(int i=0;; i++) {
		char *p=host->h_addr_list[i];
		if(NULL==p)break;
		memcpy(&(PcAddr.S_un.S_addr),p,host->h_length);
		return inet_ntoa(PcAddr);
	}
	printf("Get IP Failed!");
	exit(-1);
}
struct point {
	string name;
	SOCKET con;
};
#define po(name,con) (point){name,con}
vector<point>v;
const std::string html_response =  
    "HTTP/1.1 200 OK\r\n"  
    "Content-Type: text/html\r\n"  
    "Content-Length: 61\r\n"  
    "\r\n"  
    "<!DOCTYPE html><html><body><h1>Hello, World!</h1></body></html>";  
  
DWORD WINAPI child_thread(LPVOID V_sock) {
	SOCKET con=(SOCKET)V_sock;
//	send(new_socket, html_response.c_str(), html_response.size(), 0);  
	char rev[1500],tmp1[15];
	Sleep(3000);
//	int time=-1000;
//	while(1) {
//		int y=recv(con,rev,1,0);
//		if(y<=0) {
////			for(int i=0; i<(int)v.size(); i++)if(v[i].name==tmp1)v.erase(v.begin()+i);
////			string msg=(string)"&["+tmp1+" left]";
////			const char *c=msg.c_str();
////			for(int i=0; i<(int)v.size(); i++)send(v[i].con,c,msg.size()+1,0);
//			return 0;
//		}
//		rev[y]=0;
//		cout<<rev;
	send(con,html_response.c_str(),html_response.size(),0);
//	}
//		char cmp=rev[0];
//		if(cmp=='L') {
//			recv(con,tmp1,15,0);
//			int bj=0;
//			for(int i=0; i<(int)v.size(); i++)
//				if(v[i].name==tmp1) {
//					send(con,"!Nickname taken!",15,0);
//					bj=1;
//					break;
//				}
//			if(bj)continue;
//			send(con,"L",2,0);
//			v.push_back(po(tmp1,con));
//			string msg=(string)"&["+tmp1+" joined]";
//			const char *c=msg.c_str();
//			for(int i=0; i<(int)v.size(); i++)send(v[i].con,c,msg.size()+1,0);
//		}
//		if(cmp=='S') {
//			int y=recv(con,rev,1400,0);
//			rev[y]=0;
//			string msg=(string)"&["+tmp1+"]:"+rev;
//			const char *c=msg.c_str();
//			for(int i=0; i<(int)v.size(); i++)send(v[i].con,c,msg.size()+1,0);
//		}
//		if(cmp=='<') {
//			if(time>clock())continue;
//			time=clock()+1000;
//			send(con,".",1,0);
//			for(int i=0; i<(int)v.size(); i++) {
//				string q=">"+v[i].name;
//				send(con,q.c_str(),q.size(),0);
//			}
//		}
//		if(cmp=='K')for(int i=0; i<(int)v.size(); i++)if(v[i].con!=con)send(v[i].con,"K",1,0);
//	}
}
int main() {
	int Shock;
	char *IP=GetIp();
	cout<<"Your IP is "<<IP<<endl<<"Shock:";
	cin>>Shock;
	HWND hwnd=GetForegroundWindow();
	WSADATA wsd;
	WSAStartup(MAKEWORD(2,2),&wsd);
	SOCKET SockServer;
	sockaddr_in ServerAddr,FromAddr;
	ServerAddr.sin_family=AF_INET;
	ServerAddr.sin_port=htons(Shock);
	ServerAddr.sin_addr.S_un.S_addr=inet_addr(IP);
	SockServer=socket(AF_INET,SOCK_STREAM,0);
	if(bind(SockServer,(sockaddr*)&ServerAddr,sizeof(ServerAddr))==SOCKET_ERROR)return 1;
	if(listen(SockServer,30)==SOCKET_ERROR)return 2;
	int Socklen=sizeof(sockaddr);
	cout<<"OK. Running.."<<endl;
	Sleep(1500);
//	ShowWindow(hwnd,0);
	DWORD ls_handle_id;
	while(1) {
		SOCKET SockFrom=accept(SockServer,(sockaddr*)&FromAddr,&Socklen);
		if(SockFrom!=INVALID_SOCKET)CreateThread(NULL,0,child_thread,(LPVOID)SockFrom,0,&ls_handle_id);
	}
	return 0;
}
