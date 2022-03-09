#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h> /* Para errno */
#include <string.h> /* Para strerror() */
#include <sys/ioctl.h> /* Para ioctl() */
#include <linux/fs.h> /* Para BLKGETSIZE64 */

#include "ufuFS.h"

#define ufuRazaoInode 0.10 /* 10% de todos os blocos */
#define ufuFSComecoDoBlocoInode 1 /*Início da tabela de inodes*/

ufuFS_superBloco sb = 
{
	.tipo = ufuFS_TYPE,
	.tam_bloco = ufuFS_BLOCK_SIZE,
	.tam_inode = ufuFS_INODE_SIZE,
	.bloco_ini_inode = ufuFSComecoDoBlocoInode
};

ufuFS_INODE ufuInode; /*Todos 0's */

void writeSuperBloco(int ufuFD, ufuFS_superBloco *sb)
{
	write(ufuFD, sb, sizeof(ufuFS_superBloco));
}

void resetaInode(int ufuFD, ufuFS_superBloco *sb)
{
	int i;
	unsigned char bloco[ufuFS_BLOCK_SIZE];

	for (i = 0; i < sb->tam_bloco / sb->tam_inode; i++)
	{
		memcpy(bloco + i * sb->tam_inode, &ufuInode, sizeof(ufuInode));
	}
	for (i = 0; i < sb->tam_tabela_inode; i++)
	{
		write(ufuFD, bloco, sizeof(bloco));
	}
}

int main(int argc, char *argv[])
{
	int ufuFD;
	unsigned long long size;

	if (argc != 2)
	{
		fprintf(stderr, "Em uso: %s <arquivo da partição do dispositivo>\n", argv[0]);
		return 1;
	}
	ufuFD = open(argv[1], O_RDWR);
	if (ufuFD == -1)
	{
		fprintf(stderr, "Erro ao formatar %s: %s\n", argv[1], strerror(errno));
		return 2;
	}
	if (ioctl(ufuFD, BLKGETSIZE64, &size) == -1) /*size recebe o tamanho total do dispositivo em Bytes*/
	{
		fprintf(stderr, "Erro ao obter o tamanho de %s: %s\n", argv[1], strerror(errno));
		return 3;
	}
	sb.qtd_bloco_pen = size / ufuFS_BLOCK_SIZE;
	sb.tam_tabela_inode = sb.qtd_bloco_pen * ufuRazaoInode;
	sb.qtd_inode = sb.tam_tabela_inode * (sb.tam_bloco / sb.tam_inode);
	sb.ini_data_block = ufuFSComecoDoBlocoInode +  sb.tam_tabela_inode;

	printf("Particionando %Ld bytes da partição: %s ... ", size, argv[1]);
	fflush(stdout);
	writeSuperBloco(ufuFD, &sb);
	resetaInode(ufuFD, &sb);

	close(ufuFD);
	printf("Concluido!\n");

	return 0;
}
