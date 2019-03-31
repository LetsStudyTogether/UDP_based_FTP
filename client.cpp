#include <WinSock2.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <direct.h>
#include <io.h>

#pragma warning(disable:4996)
#pragma comment(lib,"ws2_32.lib")

using namespace std;

WSADATA wsa_data;
int  ret = WSAStartup(MAKEWORD(2, 2), &wsa_data);
SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
sockaddr_in addrServer;

int len = sizeof(SOCKADDR);



//获取当前目录下的文件
vector<string> getFiles(string cate_dir)
{
  vector<string> files;
  _finddata_t file;
  long lf;
  if ((lf = _findfirst(cate_dir.c_str(), &file)) == -1) {
    cout << cate_dir << " not found!!!" << endl;
  }
  else {
    while (_findnext(lf, &file) == 0) {
      if (strcmp(file.name, ".") == 0 || strcmp(file.name, "..") == 0)
        continue;
      files.push_back(file.name);
    }
  }
  _findclose(lf);
  sort(files.begin(), files.end());
  return files;
}

//获取当前目录（弃用）
vector<string> getCurrentDirFiles() {
  char current_address[1024];
  memset(current_address, 0, 1024);
  getcwd(current_address, 1024);
  strcat(current_address, "\\*");
  //string current = (string)current_address;
  vector<string> files = getFiles((string)current_address);
  return files;
}

//获取当前目录
string getCurrentDir() {
  char current_address[1024];
  memset(current_address, 0, 1024);
  getcwd(current_address, 1024);
  string current = (string)current_address;
  return current;
}

//输出文件列表
void giveCurrentDirFiles(vector<string> &files) {
  cout << endl;
  for (vector<string>::iterator i = files.begin(); i < files.end(); i++) {
    cout << *i << endl;
  }
}

//从服务器获取文件
void getFileFromServer(string &filename) {
  ofstream wlog("ClientLog.txt", ios::app);
  if (filename.find(" ") == string::npos) {
    cout << ">Illegal input format : " << filename << endl;
    return;
  }
  else
  {
    filename = filename.substr(filename.find(" ") + 1);
  }
  FILE *fp = fopen(filename.c_str(), "wb");
  if (!fp) {
    cout << ">Failed to create such file :" << filename << endl;
    return;
  }
  //sendto(sock, filename.c_str() , strlen(filename.c_str()), 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
  char buffer[1024];
  memset(buffer, 0x0, 1024);
  int nCount;
  int seq = 0;
  while ((nCount = recvfrom(sock, buffer, 1024, 0, (SOCKADDR*)&addrServer, &len)) > 0) {
    if (strcmp(buffer, "FileOpenFailed") == 0) {
      cout << ">Server dir doesn't exist such file : " << filename << endl;
      wlog << "Server dir doesn't exist such file : " << filename << endl;
      fclose(fp);
      remove(filename.c_str());
      return;
    }
    if (strcmp(buffer, "EndTrans") == 0)
      break;
    if (strncmp(buffer, "SEQ", 3) == 0) {
      continue;
    }
    fwrite(buffer, nCount, 1, fp);
    memset(buffer, 0x0, 1024);
    sendto(sock, "ACK", strlen("ACK"), 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
    seq++;
  }
  fclose(fp);
  if (FALSE) {
    sendto(sock, "Resend", strlen("Resend"), 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
    getFileFromServer(filename);
  }
  else
  {
    sendto(sock, "Success", strlen("Success"), 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
    cout << ">Successfully received " << filename << endl;
    wlog << "Successfully received " << filename << endl;
  }
}

//获取服务器当前目录文件列表
void getServerDirFiles() {
  ofstream wlog("ClientLog.txt", ios::app);
  char buffer[1024];
  memset(buffer, 0x0, 1024);
  int nCount;
  cout << endl;
  while ((nCount = recvfrom(sock, buffer, 1024, 0, (SOCKADDR*)&addrServer, &len)) > 0) {
    if (strcmp(buffer, "EndOfFileList") == 0)
      break;
    cout << buffer << endl;
    memset(buffer, 0x0, 1024);
  }
  wlog << "Listed server current dir files." << endl;
}

//上传文件
void sendFileToServer(string &filename) {
  ofstream wlog("ClientLog.txt", ios::app);
  if (filename.find(" ") == string::npos) {
    cout << ">Illegal input format : " << filename << endl;
    return;
  }
  else
  {
    filename = filename.substr(filename.find(" ") + 1);
  }
  FILE *fp = fopen(filename.c_str(), "rb");
  //sendto(sock, filename.c_str(), strlen(filename.c_str()), 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
  char buffer[1024];
  memset(buffer, 0x0, 1024);
  recvfrom(sock, buffer, 1024, 0, (SOCKADDR*)&addrServer, &len);
  if (strcmp(buffer, "FileAlreadyExist") == 0) {
    cout << ">Server current dir already exist a same name file. Upload file failed." << endl;
    wlog << "Server current dir already exist a same name file. Upload file failed." << endl;
    return;
  }
  if (!fp) {
    cout << ">No Such File :" << filename << endl;
    wlog << "Client has no such file :" << filename << endl;
    sendto(sock, "NoSuchFile", strlen("NoSuchFile"), 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
    return;
  }
  memset(buffer, 0x0, 1024);
  int nCount;
  int seq = 0;
  while ((nCount = fread(buffer, 1, 1024, fp)) > 0) {
  resend:
    char num[1024] = { 0 };
    char seq_buff[1024] = { 0 };
    strcat(seq_buff, "SEQ");
    strcat(seq_buff, itoa(seq, num, 10));
    sendto(sock, seq_buff, strlen(seq_buff), 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
    sendto(sock, buffer, nCount, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
    memset(buffer, 0x0, 1024);
    recvfrom(sock, buffer, 1024, 0, (SOCKADDR*)&addrServer, &len);
    if (strcmp(buffer, "ACK") == 0) {
      seq++;
      continue;
    }
    else
      goto resend;
  }
  sendto(sock, "EndTrans", strlen("EndTrans"), 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
  memset(buffer, 0x0, 1024);
  recvfrom(sock, buffer, 1024, 0, (SOCKADDR*)&addrServer, &len);
  fclose(fp);
  if (strcmp(buffer,"Success")!=0) {
    sendFileToServer(filename);
  }
  else
  {
    cout << ">Successfully uploaded " << filename << endl;
    wlog << "Successfully uploaded " << filename << endl;
  }

}

//改变服务器当前目录
void changeServerDir(string &input) {
  ofstream wlog("ClientLog.txt", ios::app);
  //sendto(sock, input.c_str(), strlen(input.c_str()), 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
  char buffer[1024];
  memset(buffer, 0x0, 1024);
  recvfrom(sock, buffer, 1024, 0, (SOCKADDR*)&addrServer, &len);
  if (strcmp(buffer, "IllegalInput") == 0) {
    cout << ">Illegal input." << endl;
    return;
  }
  else if (strcmp(buffer, "IllegalDir") == 0) {
    cout << ">Illegal dir." << endl;
    return;
  }
  else {
    cout << ">Successfully changed dir" << endl;
    wlog << "Successfully changed dir" << endl;
  }
}

int main()
{

  addrServer.sin_family = AF_INET;
  addrServer.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
  addrServer.sin_port = htons(8001);

  //sendto(sock, "HELLO", strlen("HELLO"), 0, (SOCKADDR*)&addrServer, len);

  while (1) {
    char buff[1024];
    cout << "|-----------------Client----------------|" << endl;
    cout << "|ls  --------------list server dir files|" << endl;
    cout << "|cls --------------list client dir files|" << endl;
    cout << "|get FILENAME ----get a file from server|" << endl;
    cout << "|send FILENAME ----send a file to server|" << endl;
    cout << "|cd DIR -----------enter a server folder|" << endl;
    cout << "|cd .. ------------go back to parent dir|" << endl;
    cout << "|---------------------------------------|" << endl;
    cout << ">";
    memset(buff, 0x0, 1024);
    cin.getline(buff, 1024);
    int sendlen = sendto(sock, buff, 1024, 0, (SOCKADDR*)&addrServer, len);

    if (sendlen < 0)
    {
      int error = WSAGetLastError();
      printf("error = %d\n", error);
    }

    char recvBuf[1024];
    if (strncmp(buff, "get",3) == 0) {
      string filename = buff;
      getFileFromServer(filename);
    }
    else if (strncmp(buff, "send",4) == 0) {
      string filename = buff;
      sendFileToServer(filename);
    }
    else if (strcmp(buff, "ls") == 0) {
      getServerDirFiles();
    }
    else if (strcmp(buff, "cls") == 0) {
      giveCurrentDirFiles(getCurrentDirFiles());
    }
    else if (strncmp(buff, "cd",2) == 0) {
      string filename = buff;
      changeServerDir(filename);
    }
    else
    {
      cout << "Illegal Input..." << endl; 
    }
    memset(buff, 0x0, 1024);
    cout << endl;
  }
  closesocket(sock);
  WSACleanup();
  system("pause");
  return 0;
}
