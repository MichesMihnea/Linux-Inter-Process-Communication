#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/shm.h>
#include <sys/mman.h>

int pipe_fails_nr = 0;
int shm_error = 0;
off_t file_size;
int read_fails = 0;
char *data = NULL;
char *logical = NULL;
char pipe_fails[100];
int map_fails = 0;
int shmId;
int *size;
unsigned int *pInt = NULL;
int main()
{
    if (mkfifo("RESP_PIPE_77649", 0600) < 0)
    {
        strcpy(pipe_fails, "cannot create the response pipe ");
        pipe_fails_nr ++;
    }

    int fdr = open("REQ_PIPE_77649", O_RDONLY);
    int fdw = open("RESP_PIPE_77649", O_WRONLY);

    if(fdr < 0 || fdw < 0)
    {
        if(pipe_fails_nr > 0)
            strcpy(pipe_fails, "| ");
        strcpy(pipe_fails, "cannot open the request pipe ");
        pipe_fails_nr ++;
    }
    else
    {
        int *nr;
        nr = malloc(sizeof(int));
        *nr = 7;
        write(fdw, nr, 1);
        write(fdw, "CONNECT", 7);
    }

    if(pipe_fails_nr)
        printf("ERROR\n%s\n", pipe_fails);
    else
    {
        printf("SUCCESS\n");
        while(1)
        {
            int *n;
            char buf[100];
            n = malloc(sizeof(int));
            read(fdr, n, 1);
            if(*n > 0)
                read(fdr, buf, *n );
            //printf("\n%s\n",buf);
            if(strcmp(buf, "PING") == 0)
            {
                int *nr;
                nr = malloc(sizeof(int));
                *nr = 4;
                write(fdw, nr, 1);
                write(fdw, "PING", 4);
                *nr = 4;
                write(fdw, nr, 1);
                write(fdw, "PONG", 4);
                *nr = 77649;
                write(fdw, nr, 4);
            }
            else if(strstr(buf, "EXIT"))
            {
                unlink("RESP_PIPE_77649");
                if(pInt)
                    shmdt(pInt);
                close(fdr);
                exit(0);
            }
            else if(strstr(buf, "CREATE_SHM"))
            {
		shm_error = 0;

                size = malloc(sizeof(int));
                read(fdr, size, 4);

                shmId = shmget(12818, *size, IPC_CREAT|0664);

                if (shmId < 0)
                {
                    shm_error ++;
                }
                else
                {
                    pInt = (unsigned int*) shmat(shmId, 0, 0);
                    if (*pInt == -1)
                    {
                        shm_error ++;
                    }
                }

                int *message_size;
                message_size = malloc(sizeof(int));
                *message_size = 10;
                write(fdw, message_size, 1);
                write(fdw, "CREATE_SHM", 10);

                if(shm_error)
                {
                    int *error_size;
                    error_size = malloc(sizeof(int));
                    *error_size = 5;
                    write(fdw, error_size, 1);
                    write(fdw, "ERROR", 5);
                }
                else
                {
                    *message_size = 7;
                    write(fdw, message_size, 1);
                    write(fdw, "SUCCESS", 7);
                }
            }
            else if(strstr(buf, "WRITE_TO_SHM"))
            {
                int *offset, *value;
                offset = malloc(sizeof(unsigned int));
                value = malloc(sizeof(unsigned int));
                *offset = 0;
                *value = 0;
                read(fdr, offset, 4);
                read(fdr, value, 4);

                int *message_size;
                message_size = malloc(sizeof(int));
                *message_size = 12;
                write(fdw, message_size, 1);
                write(fdw, "WRITE_TO_SHM", 12);

                if(*offset > *size - 3 || *offset < 0 || !pInt)
                {
                    *message_size = 5;
                    write(fdw, message_size, 1);
                    write(fdw, "ERROR", 5);
                }
                else
                {
                    unsigned int *pIntStart;
                    char *a;
                    pIntStart = &pInt[ *offset / sizeof(unsigned 				int) ] ;
                    a = (char*)(pIntStart -1);
                    pIntStart =  (unsigned int*)&a[6];
                    *pIntStart = *value;
                    *message_size = 7;
                    write(fdw, message_size, 1);
                    write(fdw, "SUCCESS", 7);
                }
            }
            else if(strstr(buf, "MAP_FILE"))
            {
                int *message_size;
                message_size = malloc(sizeof(int));
                read(fdr, message_size, 1);
                char filename[50];
                read(fdr, filename, *message_size);
                char path[50];
                getcwd(path, sizeof(path));
                strcat(path, "/");
                strcat(path, filename);
                *message_size = 8;
                write(fdw, message_size, 1);
                write(fdw, "MAP_FILE", 8);
                int fileDesc = open(path, O_RDONLY);
                if (fileDesc == -1)
                {
                    map_fails ++;
                }
                file_size = lseek(fileDesc, 0, SEEK_END);
                lseek(fileDesc, 0, SEEK_SET);
                data = (char*)mmap(NULL, file_size, PROT_READ, MAP_SHARED, fileDesc, 0);
                if(data == (void*)-1)
                {
                    map_fails ++;
                    close(fileDesc);
                }
                if(map_fails)
                {
                    *message_size = 5;
                    write(fdw, message_size, 1);
                    write(fdw, "ERROR", 5);
                }
                else
                {
                    *message_size = 7;
                    write(fdw, message_size, 1);
                    write(fdw, "SUCCESS", 7);
                }
            }else if(strstr(buf, "READ_FROM_FILE_OFFSET"))
            {
                read_fails = 0;
                unsigned int *offset, *nobytes;
                offset = malloc(sizeof(unsigned int));
                nobytes = malloc(sizeof(unsigned int));
                read(fdr, offset, 4);
                read(fdr, nobytes, 4);

                if(*offset + *nobytes > file_size)
                {
                    printf("\nSIZE FAIL\n");
                    read_fails ++;
                }
                if(pInt == NULL)
                {
                    printf("\nMEMORY FAIL\n");
                    read_fails ++;
                }
                if(data == NULL)
                {
                    printf("\nFILE FAIL\n");
                    read_fails ++;
                }

                if(!read_fails)
                {
                char *pFile;
                pFile = &data[( *offset) ] ;
                char *pShared = (char *) pInt;


                for(int i = 0; i < *nobytes; i ++)
                {
                    *pShared = *pFile;
                    pShared ++;
                    pFile ++;
                }

                }


                int *message_size;
                message_size = malloc(sizeof(int));

                *message_size = 21;
                write(fdw, message_size, 1);
                write(fdw, "READ_FROM_FILE_OFFSET", 21);

                if(!read_fails)
                {
                *message_size = 7;
                write(fdw, message_size, 1);
                write(fdw, "SUCCESS", 7);
                }
                else
                {
                    *message_size = 5;
                    write(fdw, message_size, 1);
                    write(fdw, "ERROR", 5);
                }

            }else if(strstr(buf, "READ_FROM_FILE_SECTION"))
			{
				read_fails = 0;
				int *section, *offset, *nobytes;
				section = malloc(sizeof(int));
				offset = malloc(sizeof(int));
				nobytes = malloc(sizeof(int));

				read(fdr, section, 4);
				read(fdr, offset, 4);
				read(fdr, nobytes, 4);

				char *pFile = data;

				pFile += 7;
				int total_sections = *pFile & 0x00FF;

				int *message_size;
				int *section_offset;
                message_size = malloc(sizeof(int));

                *message_size = 22;
                write(fdw, message_size, 1);
                write(fdw, "READ_FROM_FILE_SECTION", 22);

				if(*section > total_sections)
					read_fails ++;

				if(read_fails == 0)
				{
				pFile ++;
				for(int curr_sect = 1; curr_sect <= total_sections; curr_sect ++)
				{
					pFile += 19;
					char sect_type[4];
					sprintf(sect_type, "%d", *pFile);
					if(atoi(sect_type) != 24 && atoi(sect_type) != 46 && atoi(sect_type) != 31 && atoi(sect_type) != 43 && atoi(sect_type) != 67 && atoi(sect_type) != 40 && atoi(sect_type) != 46)
					{
						read_fails ++;
						printf("SECTION ERROR\n");
						break;
					}
					pFile ++;

					if(curr_sect == *section)
					{
					section_offset = (int*) pFile;
					}
					pFile += 8;
				}

				}

				if(read_fails)
				{
					*message_size = 5;
               	 	 write(fdw, message_size, 1);
               		 write(fdw, "ERROR", 5);
				}
				else
				{
					pFile = &data[( *section_offset + *offset) ] ;
                	char *pShared = (char *) pInt;


                for(int i = 0; i < *nobytes; i ++)
                {
                    *pShared = *pFile;
                    pShared ++;
                    pFile ++;
                }
					*message_size = 7;
               	 	 write(fdw, message_size, 1);
               		 write(fdw, "SUCCESS", 7);
				}

			}else if(strstr(buf, "READ_FROM_LOGICAL_SPACE_OFFSET"))
			{
                if(!logical)
                    logical = malloc(file_size);
                int *logical_offset, *nobytes;
                read_fails = 0;
                logical_offset = malloc(sizeof(int));
                nobytes = malloc(sizeof(int));

                read(fdr, logical_offset, 4);
                read(fdr, nobytes, 4);



                char *pFile = data;

				pFile += 7;
				int total_sections = *pFile & 0x00FF;

				int *section_offset;
				int *section_size;
				int logical_section;

				int *message_size;
                message_size = malloc(sizeof(int));
                *message_size = 30;
                write(fdw, message_size, 1);
                write(fdw, "READ_FROM_LOGICAL_SPACE_OFFSET", 30);

                if(*logical_offset + *nobytes > file_size)
                    read_fails ++;

                pFile ++;

                if(logical[0] == 0)
                {
                for(int curr_sect = 1; curr_sect <= total_sections; curr_sect ++)
				{
                    char *logical_curr;

                    logical_curr = logical;
                    if(!logical_curr)
                        printf("FATAL");

					pFile += 19;
					char sect_type[4];
					sprintf(sect_type, "%d", *pFile);
					if(atoi(sect_type) != 24 && atoi(sect_type) != 46 && atoi(sect_type) != 31 && atoi(sect_type) != 43 && atoi(sect_type) != 67 && atoi(sect_type) != 40 && atoi(sect_type) != 46)
					{
						read_fails ++;
						printf("SECTION ERROR\n");
						break;
					}

					pFile ++;

					section_offset = (int*) pFile;

					if(!section_offset)
                        printf("FATAL");

					pFile += 4;

					section_size = (int*) pFile;

					if(!section_size)
                        printf("FATAL");

					//printf("\n%d %d\n", *section_offset, *section_size);

                    logical_curr += 3072 * logical_section;

                    if(!logical_curr)
                        printf("FATAL");

                    //printf("\n%d\n", logical_section);

					char *file_byte;
					file_byte = &data[*section_offset];

					if(!file_byte)
                        printf("FATAL");

                    int i = 0;

                    while(i < *section_size)
                    {
                        *(logical + 3072 * logical_section + i) = *file_byte;
                        file_byte ++;
                        i ++;
                    }

                    logical_section += *section_size / 3072 + 1;

					pFile += 4;
				}

                }
                char *pMem;
                pMem = (char*) pInt;
                int i = 0;

				while(i < *nobytes)
				{
                    *(pMem + i) = *(logical + i + *logical_offset);
                    i ++;
				}

				if(read_fails)
                {
                    *message_size = 5;
                    write(fdw, message_size, 1);
                    write(fdw, "ERROR", 5);
                }
                else
                {
                    *message_size = 7;
                    write(fdw, message_size, 1);
                    write(fdw, "SUCCESS", 7);
                }

			}

        }

    }
    unlink("RESP_PIPE_77649");

    return 0;
}
