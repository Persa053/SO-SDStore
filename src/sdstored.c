//servidor
//./bin/sdstore proc-file teste.txt bli.txt gcompress gdecompress

//./bin/sdstored sdstored.conf bin/SDStore-transf/
//./bin/sdstore proc-file teste.txt pipe.txt gcompress gdecompress encrypt decrypt

#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <signal.h>

#define MAX_LINE_SIZE 1024

//struct Pedidos
typedef struct listaP *ListaP;


//struct Transformações
typedef struct listaT *ListaT;

ListaT listaTransf = NULL;
ListaP pendentes = NULL;
ListaP execucao = NULL;

int comandoSize = 0;
int numeroTarefa = 1;


//struct Pedidos
struct listaP {
  int numeroTarefa;
  char* tarefa;
  char* OutFIFO;
  struct listaP *prox;
};


//struct Transformações
struct listaT {
  int max;
  int curr; //current
  char* executavel;
  int *pids;
  struct listaT *prox;
};


void help(int fdwr) {
    char help[] = "./sdstored status\n./sdstored proc-file input-filename output-filename transf-id-1 transf-id-2 ...\n";
    write(fdwr, help, sizeof(help));
}





char** separaString(char* buffer){
    char** comando = NULL;
    int nrComandos = 1;
    
    for (int i = 0; buffer[i]!='\0'; i++)
    {
        if (buffer[i] == ' ') nrComandos++;
    }

    buffer = strtok(buffer, "\0");
    
    comando = (char**) realloc(comando, (nrComandos + 1) * sizeof(char*));

    char* token = strtok(buffer," ");
    comando[0] = strdup(token);

    int nrComando = 1; 
    while ((token = strtok(NULL," ")) != NULL)
    {
        comando[nrComando] = strdup(token);
        nrComando++;
    }
    comandoSize = nrComandos;
    return comando;
}

    ssize_t readln (int fd, char *buffer, size_t size) {

    int resultado = 0, i = 0;

    while ((resultado = read (fd, &buffer[i], 1)) > 0 && i < size) {
        if (buffer[i] == '\n') {
            i += resultado;
            return i;
        }

        i += resultado;
    }

    return i;
}

ListaT adicionaT(int max, char* executavel,ListaT l) {
    ListaT aux = l;
    ListaT new = malloc(sizeof(ListaT));
    new->max = max;
    new->curr = 0;
    new->executavel = malloc(sizeof(char) * strlen(executavel));
    new->pids = malloc(sizeof(int)*max);
    strcpy(new->executavel,executavel);
    new->prox = NULL;

    if (!l) {
        return new;
    }

    for (; aux->prox; aux = aux->prox);

    aux->prox = new;

    return l;
}

ListaP adicionaP (int max, char* tarefa, ListaP l) {
    
    ListaP aux = l;
    ListaP new = malloc(sizeof(struct listaP));
    new->numeroTarefa = numeroTarefa;
    new->tarefa = malloc(sizeof(char) * strlen(tarefa));
    strcpy(new->tarefa,tarefa);
    new->prox = NULL;

    if (!l) {
        return new;
    }

    for (; aux->prox; aux = aux->prox);

    aux->prox = new;

    return l;
}



void lerConfig(const char* file) {
    char *buffer = malloc(1024 * sizeof(char));
    char *exec;
    char *max;
    int fd_config = open(file, O_RDONLY);
    int n;
    char* token;


    while ((n = readln(fd_config,buffer,1024 * sizeof(char))) > 0) {
        token = strtok(buffer," ");
        exec = strdup(token);
        token = strtok(NULL," ");
        max = strdup(token);

        //printf("%s %s %s\n", filtro,exec,max);
        listaTransf = adicionaT(atoi(max),exec,listaTransf);
    }
}





int executar(char* transFolder ,char* input,char* output,char** transf,int numTransf) {
    char* path = malloc(1024 * sizeof(char));
    strcpy(path, transFolder);

//   printf("%d %d\n", fd_output, fd_input);

    int n = numTransf;                         // número de comandos == número de filhos
    int p[n-1][2];                              // -> matriz com os fd's dos pipes
    int status[n];                              // -> array que guarda o return dos filhos


    if(numTransf == 1) {
        transf[0] = strcat(path,transf[0]);
        int fd_output = open(output, O_CREAT | O_WRONLY | O_TRUNC, 0600);
        int fd_input = open(input, O_RDONLY);
        dup2(fd_input,0);
        close(fd_input);
        dup2(fd_output,1);
        close(fd_output);
    
        
        execlp(transf[0],transf[0],NULL);

        return 0;
    }

    //proc-file teste.txt bli.txt gcompress gdecompress
    // criar processos filhos para executar cada um dos comandos
    for (int i = 0; i < n; i++) {

        if (i == 0) {
            
            if (pipe(p[i]) == -1) {
                perror("Pipe não foi criado");
                return -1;
            }
            
            switch(fork()) {
                case -1:
                    perror("Fork não foi efetuado");
                    return -1;
                case 0:
                    // codigo do filho 0
                    transf[i] = strcat(path,transf[i]);
                    
                    close(p[i][0]);

                    dup2(p[i][1],1);
                    close(p[i][1]);

                    int fd_input = open(input, O_RDONLY);
                    dup2(fd_input,0);
                    close(fd_input);

                    execlp(transf[i],transf[i],NULL);

                    _exit(0);
                default:
                    close(p[i][1]);
            }
        }
        else if (i == n-1) {

            switch(fork()) {
                case -1:
                    perror("Fork não foi efetuado");
                    return -1;
                case 0:
                    // codigo do filho n-1

                    
                    transf[i] = strcat(path,transf[i]);
                    dup2(p[i-1][0],0);
                    close(p[i-1][0]);

                    int fd_output = open(output, O_CREAT | O_WRONLY | O_TRUNC, 0600);
                    dup2(fd_output,1);
                    close(fd_output);

                    execlp(transf[i],transf[i],NULL);

                    _exit(0);
                default:
                    close(p[i-1][0]);
            }
        }
        else {

            if (pipe(p[i]) == -1) {
                perror("Pipe não foi criado");
                return -1;
            }
            switch(fork()) {
                case -1:
                    perror("Fork não foi efetuado");
                    return -1;
                case 0:
                    // codigo do filho i


                    transf[i] = strcat(path,transf[i]);
                    close(p[i][0]);
                    dup2(p[i-1][0],0);
                    close(p[i-1][0]);

                    dup2(p[i][1],1);
                    close(p[i][1]);


                    execlp(transf[i],transf[i],NULL);

                    _exit(0);
                default:
                    close(p[i-1][0]);
                    close(p[i][1]);
            }
        }

    }

    for (int i = 0; i < n; i++)
    {
        wait(&status[i]);

        if (WIFEXITED(status[i])) {
            // printf("[PAI]: filho terminou com %d\n", WEXITSTATUS(status[i]));
        }
    }
    return 1;
}


int fdClienteServidor, fdServer_Server;

void terminaServer(int signum){
    
    switch (signum){
    case SIGQUIT:
        close(fdServer_Server);
        close(fdClienteServidor);
        unlink("fifo_Clients_Server");
        break;
    }
}


char* itoa(int i){
    char const digit[] = "0123456789";
    int p = 0;
    char *b = malloc(sizeof(char) * 10);
    int shifter = i;

    do{
        p++;
        shifter = shifter/10;
    }while(shifter);

    b[p] = '\0';

    do{
        p--;
        b[p] = digit[i%10];
        i = i/10;
    }while(i);

    return b;
}


void printListaTransf(ListaT l, int fd) {

    ListaT aux = l;
    int primeiro = 0;
    char* buffer = malloc(MAX_LINE_SIZE);
    for (; aux; aux = aux->prox) {
      if (primeiro == 0) {
        strcpy(buffer,"transf ");
        primeiro++;

      } else
        strcat(buffer,"transf ");

      strcat(buffer,aux->executavel);
      strcat(buffer,": ");
      strcat(buffer,itoa(aux->curr));
      strcat(buffer,"/");
      strcat(buffer,itoa(aux->max));
      strcat(buffer, " (running/max)\n");
    }
    write(fd, buffer, MAX_LINE_SIZE);
}


//./bin/sdstored sdstored.conf bin/SDStore-transf/
//proc-file teste.txt pipe.txt gcompress gdecompress encrypt decrypt


int main(int argc, char const *argv[]) {


    if (mkfifo("fifo_Clients_Server", 0666))
        perror("Mkfifo");
        
    signal(SIGQUIT, terminaServer);

    char *buffer = malloc(1024 * sizeof(char));
    char **comando;

    char* transFolder = strdup(argv[2]);

    lerConfig (argv[1]);

    if((fdClienteServidor = open("fifo_Clients_Server", O_RDONLY)) == -1){
            perror("Error opening fifo_Clients_Server1\n");
        }

    if ((fdServer_Server = open("fifo_Clients_Server", O_WRONLY)) == -1) {
        perror("Error opening fifo_Clients_Server\n");
        return -1;
        }

    while (read(fdClienteServidor, buffer, 1024 * sizeof(char)) > 0){

        comandoSize = 0;
        comando = separaString(buffer);
        memset(buffer,0,MAX_LINE_SIZE); //Limpa o espaço de memória das strings usadas
        int pid;
        int fdServidorCliente = open(comando[0], O_WRONLY);

        if(!strcmp(comando[1], "status")) {
            if ((pid = fork()) == 0){
            //printLista(pendentes,fdServidorCliente);
            printListaTransf(listaTransf,fdServidorCliente);
            }
                        
        } 
                
        else if(!strcmp(comando[1], "info")) {
            if ((pid = fork()) == 0) help(fdServidorCliente);
            }



        else if(!strcmp(comando[1],"proc-file") ){
            write(fdServidorCliente, "pending\n", 9);
            //pendentes = adicionaTarefa(pid,atoi(comando[comandoSize - 1]),numeroTarefa,tarefa,pendentes);
            
            if ((pid = fork()) == 0){
                printf("%s\n", comando[0]);                
                
                char** transformacoes = NULL; 
                char* tarefa = malloc(1024 * sizeof(char));
                strcpy(tarefa,comando[0]);
                int i = 1;
                    
                    while(i < comandoSize) { // - 1 era o que dava mal
                        if(i > 3) {
                            transformacoes = (char**) realloc(transformacoes, (i + 1) * sizeof(char*)); //(i + 1) * sizeof(char*)
                            transformacoes[i - 4] = strdup(comando[i]);  
                        }

                        strcat(tarefa," ");
                        strcat(tarefa,comando[i]);
                        i++;
                    }

                    

                    //proc-file teste.txt bli.txt gcompress gdecompress
                    // ver filhos -exec set follow-fork-mode child
                        write(fdServidorCliente, "processing...\n", 15);
                        executar(transFolder, comando [2], comando[3], transformacoes, comandoSize - 4);
                        write(fdServidorCliente, "concluded\n", 11);
                        _exit(0);
                    
                    
            }

        }
        close(fdServidorCliente);
    }
    return 0;
}
    



