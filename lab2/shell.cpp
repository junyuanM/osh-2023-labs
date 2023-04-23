#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>
#include <sys/wait.h>
#include <sstream>
#include <climits>
#include <fcntl.h>
#include <signal.h>
#include <fstream>
// Print the current working directory
void shell_pwd() {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        std::cout << cwd << std::endl;
    } else {
        perror("getcwd() error");
    }
}

// Change the current working directory
void shell_cd(std::string path) {
    if (path.empty()) {
      path = "/home";
    }
    if (chdir(path.c_str()) != 0) {
        perror("chdir() error");
    }
}

// Wait for all background processes to finish
void shell_wait(std::vector<pid_t> &bg_processes) {
    for (pid_t pid : bg_processes) {
        waitpid(pid, NULL, 0);
    }
    bg_processes.clear();
}


  void sighandler(int sig) {
    if (sig == SIGINT) {
        waitpid(0, nullptr, 0);
    }
}

void history_n(int n) {
    std::ifstream history_read("history.txt");
    std::vector<std::string> lines;
    std::string line;
    while (getline(history_read, line)) {
        lines.push_back(line);
    }
    history_read.close();

    int num_lines = lines.size();
    int start_index = num_lines - n;
    if (start_index < 0) {
        start_index = 0;
    }

    for (int i = start_index; i < num_lines; i++) {
        std::cout << i+1 << " " << lines[i] << std::endl;
    }
}
std::string history_last() {
    std::ifstream history_read("history.txt");
    std::string last_line;
    std::string line;
    while (getline(history_read, line)) {
        last_line = line;
    }
    history_read.close();
    if (last_line.empty()) {
        std::cout << "history.txt is empty" << std::endl;
    }
    return last_line;
}
std::string history_nth(int n) {
    std::ifstream history_read("history.txt");
    std::string line;
    if (n <= 0) {
            std::cout << "Line " << n << " does not exist in history.txt" << std::endl;
            return "";
        }
    for (int i = 1; i <= n; i++) {
        if (!getline(history_read, line)) {
            std::cout << "Line " << n << " does not exist in history.txt" << std::endl;
            return "";
        }
    }
    history_read.close();
    std::cout << n << " " << line << std::endl;
    return line;
}
std::vector<std::string> split(std::string s, const std::string &delimiter);

int main() {
  // 不同步 iostream 和 cstdio 的 buffer
  std::ios::sync_with_stdio(false);
//when running shell, reset history.txt
  std::ofstream history("history.txt", std::ios::out | std::ios::trunc);
  history.close();

  signal(SIGINT, sighandler);
  // 用来存储读入的一行命令
  std::string cmd;
  std::vector<pid_t> bg_processes; // vector to store background processes
  int line_n = 0;
  while (true) {
    // 打印提示符
    std::cout << "# ";
    std::string cmd;
    // 读入一行。std::getline 结果不包含换行符。
    std::getline(std::cin, cmd);
    
    if (std::cin.eof()) { // check for ^d input
    	shell_wait(bg_processes);
        return(0); // exit program
    }
    
    std::vector<std::string> args = split(cmd, " ");
    
    //judge instruction "!!" or "!n" or up or down
    if(args[0] == "!!"){
        cmd = history_last();
        args = split(cmd, " ");
    }else if(args[0][0] == '!' && args[0][1] != '!'){
        cmd = history_nth((int)args[0][1]-48);
        args = split(cmd, " ");
    }else if(args[0] == "\033[A"){//input up,then print and execute the previous instruction
        line_n = line_n - 1;
        cmd = history_nth(line_n);
        args = split(cmd, " ");
    }else if(args[0] == "\033[B"){//input down,then print and execute the next instruction
        line_n = line_n + 1;
        cmd = history_nth(line_n);
        args = split(cmd, " ");
    } else {
std::ofstream history_append("history.txt", std::ios::out | std::ios::app);
        history_append << cmd << std::endl;
        history_append.close();
}
    // 没有可处理的命令
    if (args.empty()) {
      continue;
    }

    // 退出
    if (args[0] == "exit") {
      if (args.size() <= 1) {
        shell_wait(bg_processes); // wait for all background processes to finish before exiting
        return 0;
      }

      // std::string 转 int
      std::stringstream code_stream(args[1]);
      int code = 0;
      code_stream >> code;

      // 转换失败
      if (!code_stream.eof() || code_stream.fail()) {
        std::cout << "Invalid exit code\n";
        continue;
      }

      shell_wait(bg_processes); // wait for all background processes to finish before exiting
      return code;
    }

    if (args[0] == "pwd") {
      shell_pwd();
      continue;
    }

    if (args[0] == "cd") {
      if(args.size() < 2){
        shell_cd("");
      }else{
        shell_cd(args[1]);
      }
      continue;
    }
    if(args[0] == "history"){
        // std::string 转 int
      std::stringstream code_stream(args[1]);
      int code = 0;
      code_stream >> code;
        history_n(code);
        continue;
    }
    // 处理管道命令
    std::vector<std::vector<std::string>> commands;
    std::vector<std::string> current_command;
    for (auto arg : args) {
      if (arg == "|") {
        commands.push_back(current_command);
        current_command.clear();
      } else {
      	if(arg != "&")
         current_command.push_back(arg);
      }
    }
    commands.push_back(current_command);

    int num_commands = commands.size();
    int pipes[num_commands - 1][2];

    for (int i = 0; i < num_commands - 1; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe error");
            return 1;
        }
    }

    for (int i = 0; i < num_commands; i++) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork error");
            return 1;
        } else if (pid == 0) {
            // 子进程
            if (i != 0) {
                if (dup2(pipes[i - 1][0], STDIN_FILENO) == -1) {
                    perror("dup2 error");
                    return 1;
                }
            }
            if (i != num_commands - 1) {
                if (dup2(pipes[i][1], STDOUT_FILENO) == -1) {
                    perror("dup2 error");
                    return 1;
                }
            }

            // 关闭所有管道文件描述符
            for (int j = 0; j < num_commands - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            // 处理重定向
            for (int j = 0; j < commands[i].size(); j++) {
                if (commands[i][j] == ">") {
                    if (j == commands[i].size() - 1) {
                        std::cout << "syntax error near unexpected token `newline'" << std::endl;
                        return 1;
                    }
                    int fd = open(commands[i][j + 1].c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd == -1) {
                        perror("open error");
                        return 1;
                    }
                    if (dup2(fd, STDOUT_FILENO) == -1) {
                        perror("dup2 error");
                        return 1;
                    }
                    close(fd);
                    commands[i].erase(commands[i].begin() + j, commands[i].begin() + j + 2);
                    j--;
                } else if (commands[i][j] == ">>") {
                    if (j == commands[i].size() - 1) {
                        std::cout << "syntax error near unexpected token `newline'" << std::endl;
                        return 1;
                    }
                    int fd = open(commands[i][j + 1].c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
                    if (fd == -1) {
                        perror("open error");
                        return 1;
                    }
                    if (dup2(fd, STDOUT_FILENO) == -1) {
                        perror("dup2 error");
                        return 1;
                    }
                    close(fd);
                    commands[i].erase(commands[i].begin() + j, commands[i].begin() + j + 2);
                    j--;
                } else if (commands[i][j] == "<") {
                    if (j == commands[i].size() - 1) {
                        std::cout << "syntax error near unexpected token `newline'" << std::endl;
                        return 1;
                    }
                    int fd = open(commands[i][j + 1].c_str(), O_RDONLY);
                    if (fd == -1) {
                        perror("open error");
                        return 1;
                    }
                    if (dup2(fd, STDIN_FILENO) == -1) {
                        perror("dup2 error");
                        return 1;
                    }
                    close(fd);
                    commands[i].erase(commands[i].begin() + j, commands[i].begin() + j + 2);
                    j--;
                }
            }
            char *arg_ptrs[commands[i].size() + 1];
            for (auto j = 0; j < commands[i].size(); j++) {
              arg_ptrs[j] = &commands[i][j][0];
            }
            arg_ptrs[commands[i].size()] = nullptr;

            execvp(commands[i][0].c_str(), arg_ptrs);
            exit(255);
        }
        if (i != 0) {
          close(pipes[i-1][0]);
          close(pipes[i-1][1]);
        }


          if (args[args.size()-1] != "&") { // if the last argument is not "&"
            int ret = waitpid(pid, NULL, 0);
          }else{
            bg_processes.push_back(pid); // add the process to the vector of background processes
          }
      
    }
  }
}


std::vector<std::string> split(std::string s, const std::string &delimiter) {
  std::vector<std::string> tokens;
  size_t start = 0, end = 0;
  while ((end = s.find(delimiter, start)) != std::string::npos) {
    tokens.push_back(s.substr(start, end - start));
    start = end + delimiter.length();
  }
  tokens.push_back(s.substr(start));
  return tokens;
}

