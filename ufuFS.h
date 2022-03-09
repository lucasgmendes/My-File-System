
#ifndef ufuFS_H
#define ufuFS_H

#include <time.h>
#define ufuFS_TYPE 0x13090D15  /*Reconhecimento do sistema de arquivos */
#define ufuFS_BLOCK_SIZE 512 /*Tamanho de cada bloco em Bytes*/
#define ufuFS_INODE_SIZE 64 /*Tamanho de cada inode em Byts*/
#define ufuFS_FILENAME_LEN 11 /*Tamanho maximo p/ nome de arquivo*/
#define ufuFS_DATA_BLOCK ((ufuFS_INODE_SIZE - ((ufuFS_FILENAME_LEN + 1) + 4 + 2*8)) / 4)/*Qtd de blocos de dados p/ cada inode*/

typedef struct ufuFS_super_bloco{
	unsigned int tipo;		 /* reconhcer o sistema*/
	unsigned int tam_bloco;	 /* unidade de alocacao (tamanho do bloco) */
	unsigned int qtd_bloco_pen; /*tamanho da partição em blocos */
	unsigned int tam_inode;	 /*tamanho do inode em bytes */
	unsigned int tam_tabela_inode; /*tamanho da tabela de inode em blocos */ 
	unsigned int bloco_ini_inode; /*bloco de inicio da tabela de inodes*/
	unsigned int qtd_inode; 	/* quantidade de inodes */
	unsigned int ini_data_block;	/* Início do Bloco de dados */
	unsigned int reservado[ufuFS_BLOCK_SIZE / 4 - 8];/*reservado p/ completar o tamanho de um bloco*/
} ufuFS_superBloco;

typedef struct ufuFSInode{
	char nome[ufuFS_FILENAME_LEN + 1];
	unsigned int tamanho; /*Tamanho em Bytes*/
	unsigned long long horaCriacao;
	unsigned long long horaModificacao;
	unsigned int blocos[ufuFS_DATA_BLOCK];/*Blocos utilizados pelos dados de cada inode*/
} ufuFS_INODE;

#endif