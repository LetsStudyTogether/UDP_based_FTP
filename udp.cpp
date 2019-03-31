
#include <WinSock2.h>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <direct.h>
#include <io.h>

#include "udp.h"

#pragma warning(disable:4996)
#pragma comment(lib,"ws2_32.lib")

using namespace std;

WSADATA wsa_data;
int ret = WSAStartup(MAKEWORD(2, 2), &wsa_data);
SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
sockaddr_in addr;
sockaddr_in addrClient;

int g_nThreadNum = 0;
HANDLE bufferMutex = CreateSemaphore(NULL, 1, 1, NULL);

int len = sizeof(SOCKADDR);


vector<string> getFiles(string cate_dir)
{
  vector<string> files;//存放文件名
  _finddata_t file;
  long lf;
  //输入文件夹路径
  if ((lf = _findfirst(cate_dir.c_str(), &file)) == -1) {
    cout << cate_dir << " not found!!!" << endl;
  }
  else {
    while (_findnext(lf, &file) == 0) {
      //输出文件名
      //cout<<file.name<<endl;
      if (strcmp(file.name, ".") == 0 || strcmp(file.name, "..") == 0)
        continue;
      files.push_back(file.name);
    }
  }
  _findclose(lf);
  //排序，按从小到大排序
  sort(files.begin(), files.end());
  return files;
}

vector<string> getCurrentDirFiles() {
  char current_address[1024];
  memset(current_address, 0, 1024);
  getcwd(current_address, 1024);
  strcat(current_address, "\\*");

  vector<string> files = getFiles((string)current_address);
  return files;
}

string getCurrentDir() {
  char current_address[1024];
  memset(current_address, 0, 1024);
  getcwd(current_address, 1024);
  string current = (string)current_address;
  return current;
}

void giveCurrentDirFiles(vector<string> &files) {
  for (vector<string>::iterator i = files.begin(); i < files.end(); i++) {
    cout << *i << endl;
  }
}

string rootDir = getCurrentDir();
string currentDir = getCurrentDir();  //不带\*的
string fullCurrentDir = currentDir + "\\*";

//获取当前目录文件
vector<string> getDirFiles() {
  vector<string> files = getFiles(fullCurrentDir);
  files.insert(files.begin(), currentDir);
  return files;
}

//向client发送文件列表
void sendDirFilesListToClient(vector<string> &files) {
  ofstream wlog("ServerLog.txt", ios::app);
  char buffer[1024];
  memset(buffer, 0x0, 1024);
  for (vector<string>::iterator i = files.begin(); i < files.end(); i++) {
    strcpy(buffer, (*i).c_str());
    sendto(sock, buffer, 1024, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
  }
  sendto(sock, "EndOfFileList", strlen("EndOfFileList"), 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
  wlog << "Client required file list." << endl;
  cout << "Client required file list." << endl;
}

//发送文件
void sendFileToClient(string &filename) {
  ofstream wlog("ServerLog.txt", ios::app);
  string fullPath = currentDir + "\\" + filename;
  FILE *fp = fopen(fullPath.c_str(), "rb");
  if (!fp) {
    sendto(sock, "FileOpenFailed", strlen("FileOpenFailed"), 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
    wlog << "Client tried to get a non-exist file." << endl;
    cout << "Client tried to get a non-exist file." << endl;
    return;
  }
  char buffer[1024] = { 0 };  //缓冲区
  int nCount;
  int seq = 0;
  while ((nCount = fread(buffer, 1, 1024, fp)) > 0) {
  resend:
    char num[1024] = { 0 };
    char seq_buff[1024] = { 0 };
    strcat(seq_buff, "SEQ");
    strcat(seq_buff, itoa(seq, num, 10));
    sendto(sock, seq_buff, strlen(seq_buff), 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
    sendto(sock, buffer, nCount, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
    memset(buffer, 0x0, 1024);
    recvfrom(sock, buffer, 1024, 0, (SOCKADDR*)&addrClient, &len);
    if (strcmp(buffer, "ACK") == 0) {
      seq++;
      continue;
    }
    else
      goto resend;
  }
  sendto(sock, "EndTrans", strlen("EndTrans"), 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
  memset(buffer, 0x0, 1024);
  recvfrom(sock, buffer, 1024, 0, (SOCKADDR*)&addrClient, &len);
  fclose(fp);
  if (strcmp(buffer, "Success")!=0) {
    sendFileToClient(filename);
  }
  else
  {
    wlog << "Sent file to client. File name:" << filename << endl;
    cout << "Sent file to client. File name:" << filename << endl;
  }
}
//接收文件
void recvFileFromClient(string &filename) {
  ofstream wlog("ServerLog.txt", ios::app);
  vector<string> filelist = getDirFiles();
  if (find(filelist.begin(), filelist.end(), filename) != filelist.end()) {
    sendto(sock, "FileAlreadyExist", strlen("FileAlreadyExist"), 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
    wlog << "Client tried to upload an exist file." << endl;
    cout << "Client tried to upload an exist file." << endl;
    return;
  }
  sendto(sock, "Continue", strlen("Continue"), 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
  FILE *fp = fopen((currentDir+"\\"+filename).c_str(), "wb");
  char buffer[1024] = { 0 };  //文件缓冲区
  int nCount;
  int seq = 0;
  while ((nCount = recvfrom(sock, buffer, 1024, 0, (SOCKADDR*)&addrClient, &len)) > 0) {
    if (strcmp(buffer, "NoSuchFile") == 0) {
      //cout << "Client don't have such file: " << filename << endl;
      fclose(fp);
      remove((currentDir + "\\" + filename).c_str());
      wlog << "Client tried to upload a non-exist file." << endl;
      cout << "Client tried to upload a non-exist file." << endl;
      return;
    }
    if (strcmp(buffer, "EndTrans") == 0)
      break;
    if (strncmp(buffer, "SEQ", 3) == 0) {
      continue;
    }
    fwrite(buffer, nCount, 1, fp);
    memset(buffer, 0x0, 1024);
    sendto(sock, "ACK", strlen("ACK"), 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
    seq++;
  }
  fclose(fp);
  if (FALSE) {
    sendto(sock, "Resend", strlen("Resend"), 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
    recvFileFromClient(filename);
  }
  else
  {
    sendto(sock, "Success", strlen("Success"), 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
    wlog << "Received file from client. File name:" << (currentDir + "\\" + filename) << endl;
    cout << "Received file from client. File name:" << (currentDir + "\\" + filename) << endl;
  }
}

DWORD WINAPI serverThread(LPVOID IpParameter) {
  sockaddr_in *port = (sockaddr_in*)(LPVOID)IpParameter;
  sockaddr_in addrThread = *port;
  while (1) {
    char recvBuf[1024];
    memset(recvBuf, 0x0, 1024);
    int nRecv = recvfrom(sock, recvBuf, 1024, 0, (SOCKADDR*)&addrThread, &len);
    if (addrThread.sin_port != port->sin_port) {
      cout << "Change?" << endl;
      HANDLE newthread = CreateThread(NULL, 0, serverThread, LPVOID(&addrThread), 0, NULL);
      WaitForSingleObject(bufferMutex, INFINITE);
      ReleaseSemaphore(bufferMutex, 1, NULL);
      g_nThreadNum++;
      cout << g_nThreadNum << endl;
    }
    else {
      if (nRecv > 0)
      {
        cout << recvBuf << endl;
        WaitForSingleObject(bufferMutex, INFINITE);
      }
      if (strncmp(recvBuf, "get",3) == 0) {
        string filename;
        recvfrom(sock, recvBuf, 1024, 0, (SOCKADDR*)&addrThread, &len);
        filename = recvBuf;
        sendFileToClient(filename);
        memset(recvBuf, 0x0, 1024);
      }
      else if (strcmp(recvBuf, "send") == 0) {
        string filename;
        recvfrom(sock, recvBuf, 1024, 0, (SOCKADDR*)&addrThread, &len);
        filename = recvBuf;
        recvFileFromClient(filename);
        memset(recvBuf, 0x0, 1024);
      }
      else if (strcmp(recvBuf, "ls") == 0) {
        sendDirFilesListToClient(getCurrentDirFiles());
      }
      ReleaseSemaphore(bufferMutex, 1, NULL);
    }
  }
  return 0;
}

//改变目录
void dirChange(string &filename) {
  ofstream wlog("ServerLog.txt", ios::app);
  string newDir;
  if (filename.find(" ") == string::npos) {
    //cout << ">Illegal input format : " << filename << endl;
    sendto(sock, "IllegalInput", strlen("IllegalInput"), 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
    return;
  }
   filename = filename.substr(filename.find(" ") + 1);
   if (filename == "..") {
     newDir = currentDir.substr(0, currentDir.find_last_of("\\"));
     if (newDir.length() < rootDir.length()) {
       sendto(sock, "IllegalDir", strlen("IllegalDir"), 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
       wlog << "Client tried to exceed root dir. " << currentDir << endl;
       cout << "Client tried to exceed root dir." << currentDir << endl;
       return;
     }
   }
   else
   {
     newDir = currentDir + "\\" + filename;
   }
   newDir += "\\*";
   _finddata_t file;
   long lf;
   if ((lf = _findfirst(newDir.c_str(), &file)) == -1) {
     sendto(sock, "IllegalDir", strlen("IllegalDir"), 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
     wlog << "Client tried to access a non-exist dir. " << currentDir << endl;
     cout << "Client tried to access a non-exist dir." << currentDir << endl;
     return;
   }
   fullCurrentDir = newDir;
   currentDir = newDir.substr(0, newDir.find_last_of("\\"));
   sendto(sock, "DirChanged", strlen("DirChanged"), 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
   wlog << "Current dir changed to: " << currentDir << endl;
   cout << "Current dir changed to: " << currentDir << endl;
}


int main()
{

  // 创建socket
  addr.sin_family = AF_INET;
  addr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
  addr.sin_port = htons(8001);

  // 绑定
  bind(sock, (SOCKADDR*)&addr, sizeof(SOCKADDR));

  int len = sizeof(SOCKADDR);
  cout << "Server Ready." << endl;
  while (1)
  {
    char recvBuf[1024];
    int nRecv = recvfrom(sock, recvBuf, 1024, 0, (SOCKADDR*)&addrClient, &len);
    /*
    if (nRecv > 0)
    {
      cout << recvBuf << endl;
    }
    */
    if (strncmp(recvBuf, "get",3) == 0) {
      string filename = recvBuf;
      if (filename.find(" ") == string::npos) {
        cout << ">Illegal input format : " << filename << endl;
      }
      else
      {
        filename = filename.substr(filename.find(" ") + 1);
      }
      sendFileToClient(filename);
      memset(recvBuf, 0x0, 1024);
    }
    else if (strncmp(recvBuf, "send",4) == 0) {
      string filename = recvBuf;
      if (filename.find(" ") == string::npos) {
        cout << ">Illegal input format : " << filename << endl;
      }
      else
      {
        filename = filename.substr(filename.find(" ") + 1);
      }
      recvFileFromClient(filename);
      memset(recvBuf, 0x0, 1024);
    }
    else if (strcmp(recvBuf, "ls") == 0) {
      sendDirFilesListToClient(getDirFiles());
    }
    else if (strncmp(recvBuf, "cd",2) == 0) {
      string filename = recvBuf;
      dirChange(filename);
      //cout << fullCurrentDir << endl;
    }
    else
    {

    }

  }
  closesocket(sock);
  system("pause");
  return 0;
}