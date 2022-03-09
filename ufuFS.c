#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#include "ufuFS.h"

ufuFS_superBloco sb;
unsigned char *used_blocks;
unsigned char block[ufuFS_BLOCK_SIZE];

void ufuFS_create (int ufuFD, char *fn)
{
    int i;
    ufuFS_INODE i_node;

    lseek(ufuFD, sb.bloco_ini_inode * sb.tam_bloco, SEEK_SET);  /*Pula o super bloco e aponta para a tabela de inodes*/
    
    for (i = 0; i < sb.qtd_inode; i++){
        read(ufuFD, &i_node, sizeof(ufuFS_INODE));
        if(!i_node.nome[0]) break;   /*Verifica se já existe um arquivo escrito, caso não haja, da break no loop e cria o arquivo*/
        if(strcmp(i_node.nome, fn) == 0)
        {
            printf("O arquivo %s já existe!\n",fn);
            return;
        }
    }

    if(i == sb.qtd_inode)
    {
        printf("Não há mais entradas!");
        return;
    }

    lseek(ufuFD, -(off_t)(sb.tam_inode), SEEK_CUR); //Lê a partir da localização atual voltando 64 bytes (tamanho do inode)

    strncpy(i_node.nome, fn, ufuFS_FILENAME_LEN);
    
    i_node.nome[ufuFS_FILENAME_LEN] = 0;
    i_node.tamanho = 0;
    i_node.horaCriacao = time(NULL);
    i_node.horaModificacao = time(NULL);
    
    for(i=0; i < ufuFS_DATA_BLOCK; i++)
    {
        i_node.blocos[i] = 0;
    }

    write(ufuFD, &i_node, sizeof(ufuFS_INODE));
}

int ufuFS_seek(int ufuFD, char *fn, ufuFS_INODE *i_node) //PROCURA O ARQUIVO PELO NOME
{
    int i;

    lseek(ufuFD, sb.bloco_ini_inode*sb.tam_bloco, SEEK_SET);
    
    for(i = 0; i < sb.qtd_inode; i++)
    {
        read(ufuFD, i_node, sizeof(ufuFS_INODE));
        if(!i_node->nome[0]) continue;
        if (strcmp(i_node->nome, fn) == 0) return i;
    }
    return -1; //Retorna -1 se não achou o arquivo
}

int get_data_block(int ufuFD) //Reservar os blocos
{
	int i;

	for (i = sb.ini_data_block; i < sb.qtd_bloco_pen; i++)
	{
		if (used_blocks[i] == 0)
		{
			used_blocks[i] = 1;
			return i;    //Retorna o primeiro bloco livre encontrado
		}
	}
	return -1; //Retorna -1 se não há mais blocos disponíveis
}

void put_data_block(int ufuFD, int i) //Libera os blocos 
{
	used_blocks[i] = 0;
}


void ufuFS_write(int ufuFD, char *fn)
{
    int i, cur_read_i, to_read, cur_read, total_size, block_i, free_i;
    ufuFS_INODE i_node;

    if((i = ufuFS_seek(ufuFD, fn, &i_node)) == -1)
    {
        printf("Arquivo %s nao existe\n", fn);
        return;
    }

    //Liberação dos blocos previamente alocados
    for(block_i = 0; block_i < ufuFS_DATA_BLOCK; block_i++)
    {
        if(!i_node.blocos[block_i])
        {
            break;
        }
        put_data_block(ufuFD, i_node.blocos[block_i]);
    }

    cur_read_i = 0; //Já lidos
    to_read = sb.tam_bloco; //Faltam para ler
    total_size = 0;
    block_i = 0;

    while ((cur_read = read(0, block + cur_read_i, to_read)) > 0) //read retorna 0 quando não houver mais o que ler
    {
        if(cur_read == to_read)
        {
            //escrever o bloco
            if(block_i == ufuFS_DATA_BLOCK) //Tamanho limete do arquivo
                break;

            if((free_i = get_data_block(ufuFD)) == -1) //Sistema de arquivos cheio | free_i recebe o bloco livre
                break;

            lseek(ufuFD, free_i * sb.tam_bloco, SEEK_SET); /*Aponta para o início do bloco livre*/

            write(ufuFD, block, sb.tam_bloco); //Grava o conteúdo lido pelo read

            i_node.blocos[block_i] = free_i;
            block_i++;
            total_size += sb.tam_bloco;

            //Reseta as variaveis após toda leitura
            cur_read_i = 0;
            to_read = sb.tam_bloco;
        }
        else
        {
            cur_read_i += cur_read;
            to_read -= cur_read;
        }
    }
    
    if((cur_read <= 0) && (cur_read_i))
    {
        //Quando não se usa todo o bloco
        if((block_i != ufuFS_DATA_BLOCK) && ((i_node.blocos[block_i] = get_data_block(ufuFD)) != -1))
        {
            lseek(ufuFD, i_node.blocos[block_i] * sb.tam_bloco, SEEK_SET);
            write(ufuFD, block, cur_read_i);
            
            total_size += cur_read_i;
        }
    }
    
    i_node.tamanho = total_size;
    i_node.horaModificacao = time(NULL);
    
    lseek(ufuFD, sb.bloco_ini_inode * sb.tam_bloco + i * sb.tam_inode, SEEK_SET); /*Aponta para o endereço do bloco de dados do arquivo no pendrive*/
    write(ufuFD, &i_node, sizeof(ufuFS_INODE));
}

void ufuFS_read (int ufuFD, char *fn)
{
    int i, block_i, already_read, rem_to_read, to_read;
    ufuFS_INODE i_node;

    if((i = ufuFS_seek(ufuFD, fn, &i_node)) == -1)
    {
        printf("Arquivo %s nao existe!\n",fn);
        return;
    }

    already_read = 0;
    rem_to_read = i_node.tamanho;

    //Leitura em blocos
    for(block_i = 0; block_i < ufuFS_DATA_BLOCK; block_i++)
    {
        if(!i_node.blocos[block_i]) break;

        to_read = (rem_to_read >= sb.tam_bloco) ? sb.tam_bloco : rem_to_read;

        lseek(ufuFD, i_node.blocos[block_i] * sb.tam_bloco, SEEK_SET);
        read(ufuFD, block, to_read);
        write(1, block, to_read);

        already_read += to_read;
        rem_to_read -= to_read;

        if(!rem_to_read) break;
    }
    i_node.horaModificacao = time(NULL);
}

void ufuFS_list (int ufuFD)
{
    int i;
    ufuFS_INODE i_node;

    lseek(ufuFD, sb.bloco_ini_inode * sb.tam_bloco, SEEK_SET);

    for(i = 0; i < sb.qtd_inode; i++)
    {
        read(ufuFD, &i_node, sizeof(ufuFS_INODE));

        if(!i_node.nome[0]) continue;

        	printf("%-15s  %10d bytes Criacao: %s ",
			i_node.nome, i_node.tamanho,
			ctime((time_t *)&i_node.horaCriacao)
            );

            printf("Ultimo acesso: %s",ctime((time_t *)&i_node.horaModificacao));
    }
}

void inicializa_shell(int ufuFD)
{
    int i, j;
    ufuFS_INODE i_node;

    used_blocks = (unsigned char *) (calloc(sb.qtd_bloco_pen, sizeof(unsigned char)));

    if(!used_blocks)
    {
        printf("Estouro de memória\n");
        exit(1);
    }

    for(i = 0; i < sb.ini_data_block; i++) /*Marca o super bloco como já utilizado*/
    {
        used_blocks[i] = 1;
    }

    lseek(ufuFD, sb.bloco_ini_inode * sb.tam_bloco, SEEK_SET); /*Aponta para o bloco subsequente ao do super bloco*/

    for(i = 0; i < sb.qtd_inode; i++)
    {
        read(ufuFD, &i_node, sizeof(ufuFS_INODE)); /*Leitura de cada inode da tabela de inodes*/
        if(!i_node.nome[0]) continue;

        for(j = 0; j < ufuFS_DATA_BLOCK; j++)
        {
            if(i_node.blocos[j] == 0) break;
            used_blocks[i_node.blocos[j]] = 1; /*Marca os blocos de dados usados em cada inode*/
        }
    }
}

void finaliza_shell(int ufuFD)
{
    free(used_blocks);
}

void ufuFS_remove(int ufuFD, char *fn)
{
    int i, block_i;
    ufuFS_INODE i_node;

    if((i = ufuFS_seek(ufuFD, fn, &i_node)) == -1)
    {
        printf("Arquivo %s nao existe!\n",fn);
        return;
    }

    /*Libera todos os blocos alocados*/
    for(block_i = 0; block_i < ufuFS_DATA_BLOCK; block_i++)
    {
        if(!i_node.blocos[block_i])
        {
            break;
        }
        put_data_block(ufuFD, i_node.blocos[block_i]); //Libera os blocos usados pelo inode
    }

    memset(&i_node, 0, sizeof(ufuFS_INODE)); //Libera o inode

    lseek(ufuFD, sb.bloco_ini_inode * sb.tam_bloco + i * sb.tam_inode,SEEK_SET);
    
    write(ufuFD, &i_node, sizeof(ufuFS_INODE)); //salva as alterações
}

void ufuFS_read_file(int ufuFD, char *fn, char *path) //Lê do pendrive e escreve no arquivo do SO
{
    int i, block_i, already_read, rem_to_read, to_read;
    ufuFS_INODE i_node;
   
    int file;
    file = open(path, O_RDWR|O_CREAT, 0777); //Cria arquivo no SO
    if(file == -1){
        printf("Erro na criação do arquivo!\n");        
        return;
    }

    if((i = ufuFS_seek(ufuFD, fn, &i_node)) == -1) //Verifica se o arquivo existe no pendrive
    {
        printf("Arquivo %s não existe\n", fn);
        return;
    }

    already_read = 0;
    rem_to_read = i_node.tamanho;

    for(block_i = 0; block_i < ufuFS_DATA_BLOCK; block_i++)
    {
        if(!i_node.blocos[block_i]) break;

        to_read = (rem_to_read >= sb.tam_bloco) ? sb.tam_bloco : rem_to_read;

        lseek(ufuFD, i_node.blocos[block_i] * sb.tam_bloco, SEEK_SET);

        read(ufuFD, block, to_read);

        write(file, block, to_read);

        already_read += to_read;
        rem_to_read -= to_read;

        if(!rem_to_read) break;
    }

    close(file);
}


void ufuFS_write_to_file(int ufuFD, char *fn, char *path)   //Escreve o conteudo de um arquivo do SO em um arquivo no pendrive
{
    int i, cur_read_i, to_read, cur_read, total_size, block_i, free_i;
    ufuFS_INODE i_node;

    int file;
    file = open(path, O_RDONLY); /*Abre o arquivo do computador*/
    
    if(file == -1){
        printf("Erro ao abrir arquivo!\n");
        return;
    }
        
    ufuFS_create(ufuFD, fn);
   
    if((i = ufuFS_seek(ufuFD, fn, &i_node)) == -1)
    {
        printf("Arquivo não foi criado no pendrive\n",fn);
        return;
    }

    /*Libera topos blocos previamente alocados, caso exista!*/
    for(block_i = 0; block_i < ufuFS_DATA_BLOCK; block_i++)
    {   
        if(!i_node.blocos[block_i]){
            break;
        }
        put_data_block(ufuFD, i_node.blocos[block_i]);
    }

    cur_read_i = 0;
    to_read = sb.tam_bloco;
    total_size = 0;
    block_i = 0;

    while((cur_read = read(file, block + cur_read_i, to_read)) > 0)
    {
        if(cur_read == to_read)
        {   
            /*Escreve este bloco*/
            if(block_i == ufuFS_DATA_BLOCK)
                break; /*Tamanho limite do arquivo*/
            if((free_i = get_data_block(ufuFD)) == -1)
                break; /*Sistema de arquivo cheio*/

            lseek(ufuFD, free_i * sb.tam_bloco, SEEK_SET);
            write(ufuFD, block, sb.tam_bloco);

            i_node.blocos[block_i] = free_i;
            block_i++;
            total_size += sb.tam_bloco;

            /*Reseta várias variáveis*/
            cur_read_i = 0;
            to_read = sb.tam_bloco;
        }
        else
        {
            cur_read_i += cur_read;
            to_read -= cur_read;
        }
    }
    
    if((cur_read <= 0) && (cur_read_i))
    {
        /*Escrever o restante do bloco*/
        if((block_i != ufuFS_DATA_BLOCK) && ((i_node.blocos[block_i] = get_data_block(ufuFD)) != -1))
        {
            lseek(ufuFD, i_node.blocos[block_i] * sb.tam_bloco, SEEK_SET);
            write(ufuFD, block, cur_read_i);
            total_size += cur_read_i;
        }
    }

    i_node.tamanho = total_size;
    i_node.horaCriacao = time(NULL);
    i_node.horaModificacao = time(NULL);

    lseek(ufuFD, sb.bloco_ini_inode * sb.tam_bloco + i * sb.tam_inode, SEEK_SET);
    write(ufuFD, &i_node, sizeof(ufuFS_INODE));

    close(file);
}

void ufuFS_copy_to_usb(int ufuFD, char *path)
{
    char new_name[ufuFS_FILENAME_LEN];
    
    printf("Digite o (novo)nome do arquivo: ");
    scanf("%s",new_name);

    ufuFS_write_to_file(ufuFD, new_name, path);
}


void ufuFS_copy_to_so(int ufuFD, char *file_name){
    ufuFS_INODE i_node;
    int i;

    if((i = ufuFS_seek(ufuFD, file_name, &i_node)) == -1) //Verifica se o arquivo existe no pendrive
    {
        printf("Arquivo %s não existe\n", file_name);
        return;
    }

    char *path;

    printf("Digite o caminho onde sera copiado no SO:");
    scanf("%s",path);

    ufuFS_read_file(ufuFD, file_name, path);
}

void ufuFS_help(void)
{
    printf("Comandos:\n");
    printf("\thelp\tcreate <arquivo>\t delete <arquivo>\n");
    printf("\twrite <arquivo>\tread <arquivo>\tlist\n");
    printf("\tcopy_to_usb <caminho>\tcopy_to_so <arquivo>\n");
    printf("\texit\n");
}


void ufuFS_shell(int ufuFD)
{
    int finalizado;
    char cmd[256], *fn;
    int ret;

    finalizado = 0;

    printf("Bem vindo ao Shell de navegação ufuFS\n\n");
    printf("Tamanho do bloco            : %d bytes\n", sb.tam_bloco);
    printf("Tamanho da partição         : %d blocos\n", sb.qtd_bloco_pen);
    printf("Tamanho do Inode            : %d bytes\n", sb.tam_inode);
    printf("Tamanho da tabela de Inodes : %d blocos\n",sb.tam_tabela_inode);
    printf("Quantidade de Inodes        : %d\n", sb.qtd_inode);
    printf("\n");

    inicializa_shell(ufuFD);

    while(!finalizado)
    {
        printf(" $hell> ");
        ret = scanf("%[^\n]",cmd);

        if(ret < 0)
        {
            finalizado = 1;
            printf("\n");
            continue;
        }
        else
        {
            getchar();
            if(ret == 0) continue;
        }
        if(strcmp(cmd, "help") == 0)
        {
            ufuFS_help();
            continue;
        }
        else if(strcmp(cmd, "exit") == 0)
        {
            finalizado = 1;
            continue;
        }
        else if(strcmp(cmd, "list") == 0)
        {
            ufuFS_list(ufuFD);
            continue;
        }
        else if(strncmp(cmd, "create", 6) == 0)
        {
            if(cmd[6] == ' ')
            {
                fn = cmd + 7;
                while (*fn == ' ') fn++;
                if(*fn != '\0')
                {
                    ufuFS_create(ufuFD, fn);
                    continue;
                }
            }
        }
        else if (strncmp(cmd, "delete" ,6) == 0)
        {
            if(cmd[6] == ' ')
            {
                fn = cmd + 7;
                while (*fn == ' ') fn++;
                if(*fn != '\0')
                {
                    ufuFS_remove(ufuFD, fn);
                    continue;
                }
            }
        }
        else if (strncmp(cmd, "read" ,4) == 0)
        {
            if(cmd[4] == ' ')
            {
                fn = cmd + 5;
                while (*fn == ' ') fn++;
                if(*fn != '\0')
                {
                    ufuFS_read(ufuFD, fn);
                    continue;
                }
            }
        }
        else if (strncmp(cmd, "write", 5) == 0)
        {
            if(cmd[5] == ' ')
            {
                fn = cmd + 6;
                while (*fn == ' ') fn++;
                if(*fn != '\0')
                {
                    ufuFS_write(ufuFD, fn);
                    continue;
                }
            }
        }
        else if (strncmp(cmd, "copy_to_usb", 11) == 0)
        {
            char *path;
            if(cmd[11] == ' ')
            {
                path = cmd + 12;
                while (*path == ' ') path++;
                if(*path != '\0')
                {
                    ufuFS_copy_to_usb(ufuFD, path);
                    continue;
                }
            }
        }
        else if (strncmp(cmd, "copy_to_so", 10) == 0)
        {
            char *fn;
            if(cmd[10] == ' ')
            {
                fn = cmd + 11;
                while (*fn == ' ') fn++;
                if(*fn != '\0')
                {
                    ufuFS_copy_to_so(ufuFD, fn);
                    continue;
                }
            }
        }
        printf("Comando Desconhecido/Incorreto: %s\n", cmd);
        ufuFS_help();
    }
    finaliza_shell(ufuFD);
}


int main(int argc, char *argv[])
{
    char *ufuFS_file;
    int ufuFD;

    if(argc != 2)
    {
        fprintf(stderr, "Erro! %s", argv[0]);
        return 1;
    }
    
    ufuFS_file = argv[1];
    ufuFD = open(ufuFS_file, O_RDWR);

    if(ufuFD == -1)
    {
        fprintf(stderr,"Não foi possível inicializar o shell sobre %s\n",ufuFS_file);
        return 2;
    }

    read(ufuFD, &sb, sizeof(ufuFS_superBloco));

    if(sb.tipo != ufuFS_TYPE)
    {
        fprintf(stderr, "Sistema de Arquivo inválido. Finalizando...\n");
        close(ufuFD);
        return 3;
    }

    ufuFS_shell(ufuFD);
    close(ufuFD);

    return 0;
}