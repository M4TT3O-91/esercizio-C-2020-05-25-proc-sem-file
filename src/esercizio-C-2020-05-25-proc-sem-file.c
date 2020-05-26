/*
 * il processo principale crea un file "output.txt" di dimensione FILE_SIZE (all'inizio ogni byte del file deve avere valore 0)

 #define FILE_SIZE (1024*1024)

 #define N 4
 è dato un semaforo senza nome: proc_sem

 il processo principale crea N processi figli

 i processi figli aspettano al semaforo proc_sem.

 ogni volta che il processo i-mo riceve semaforo "verde", cerca il primo byte del file che abbia valore 0 e ci scrive il valore ('A' + i). La scrittura su file è concorrente e quindi va gestita opportunamente (ad es. con un mutex).

 se il processo i-mo non trova una posizione in cui poter scrivere il valore, allora termina.

 il processo padre:

 per (FILE_SIZE+N) volte, incrementa il semaforo proc_sem

 aspetta i processi figli e poi termina.

 risolvere il problema in due modi:

 soluzione A:

 usare le system call open(), lseek(), write()

 soluzione B:

 usare le system call open(), mmap()
 */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <errno.h>
#include <pthread.h>
#include <semaphore.h>

//#define FILE_SIZE (1024*1024)
#define FILE_SIZE (100)
#define N 4

sem_t *proc_sem;
sem_t *mutex;

int create_file_set_size(char *file_name, unsigned int file_size);
void soluzione_A();
void soluzione_B();
void read_write_in_file(int fd, int i);

#define CHECK_ERR(a,msg) {if ((a) == -1) { perror((msg)); exit(EXIT_FAILURE); } }
#define CHECK_ERR_MMAP(a,msg) {if ((a) == MAP_FAILED) { perror((msg)); exit(EXIT_FAILURE); } }

/******************************************************************************************/
int main(int argc, char *argv[]) {
	proc_sem = mmap(NULL, // NULL: è il kernel a scegliere l'indirizzo
			sizeof(sem_t) * 2, // dimensione della memory map
			PROT_READ | PROT_WRITE, // memory map leggibile e scrivibile
			MAP_SHARED | MAP_ANONYMOUS, // memory map condivisibile con altri processi e senza file di appoggio
			-1, 0); // offset nel file
	CHECK_ERR_MMAP(proc_sem, "mmap")

	mutex = proc_sem + 1;

	printf("ora avvio la soluzione_A()...\n");
	//soluzione_A();

	printf("ed ora avvio la soluzione_B()...\n");
	soluzione_B();

	printf("bye!\n");
	return 0;
}

void soluzione_A() {
//	usare le system call open(), lseek(), write()
	char *file_name = "output_A.txt";
	unsigned int file_size = FILE_SIZE;
	int fd = create_file_set_size(file_name, file_size);
	int res;
	pid_t pid;

	res = sem_init(proc_sem, 1, // 1 => il semaforo è condiviso tra processi, 0 => il semaforo è condiviso tra threads del processo
			0 // valore iniziale del semaforo
			);
	CHECK_ERR(res, "Sem init")

	res = sem_init(mutex, 1, // 1 => il semaforo è condiviso tra processi, 0 => il semaforo è condiviso tra threads del processo
			1 // valore iniziale del semaforo (se mettiamo 0 che succede?)
			);
	CHECK_ERR(res, "Mutex init")

	for (int i = 0; i < N; i++) {
		pid = fork();
		switch (pid) {
		case 0:
			while (1) {
				if (sem_wait(proc_sem) == -1) {
					perror("sem_wait");
					exit(EXIT_FAILURE);
				}
				read_write_in_file(fd, i);
			}
			break;
		case -1:
			perror("fork()");
			exit(EXIT_FAILURE);
		default:
			;
		}
	}
	for (int i = 0; i < FILE_SIZE + N; i++) {
		if (sem_post(proc_sem) == -1) {
			perror("sem_post");
			exit(EXIT_FAILURE);
		}
	}

	for (int j = 0; j < N; j++)
		wait(NULL);

	printf("Processo A completato BYE!\n");
	return;
}

void soluzione_B() {
//	usare le system call open(), mmap()
	char *file_name = "output_B.txt";
	unsigned int file_size = FILE_SIZE;
	char *file_map;
	int res;
	int index = 0;
	int fd = create_file_set_size(file_name, file_size);

	pid_t pid;

	file_map = mmap(NULL, // NULL: è il kernel a scegliere l'indirizzo
			file_size, // dimensione della memory map
			PROT_READ | PROT_WRITE, // memory map leggibile e scrivibile
			MAP_SHARED, // memory map condivisibile con altri processi
			fd, 0); // offset nel file

	CHECK_ERR_MMAP(file_map, "file mapping error");
	close(fd);

	res = sem_init(proc_sem, 1, // 1 => il semaforo è condiviso tra processi, 0 => il semaforo è condiviso tra threads del processo
			0 // valore iniziale del semaforo
			);
	CHECK_ERR(res, "Sem init")

	res = sem_init(mutex, 1, // 1 => il semaforo è condiviso tra processi, 0 => il semaforo è condiviso tra threads del processo
			1 // valore iniziale del semaforo (se mettiamo 0 che succede?)
			);
	CHECK_ERR(res, "Mutex init")

	for (int i = 0; i < N; i++) {
		pid = fork();
		switch (pid) {
		case 0:
			while (1) {
				if (sem_wait(proc_sem) == -1) {
					perror("sem_wait");
					exit(EXIT_FAILURE);
				}

				if (sem_wait(mutex) == -1) {
					perror("sem_wait");
					exit(EXIT_FAILURE);
				}

				while (file_map[index] != 0) {
					index++;
					if (index == FILE_SIZE) {
						if (sem_post(mutex) == -1) {
							perror("sem_post");
							exit(EXIT_FAILURE);
						}
						//printf("CHILD numero %d EXIT_SUCCESS\n", i);
						exit(EXIT_SUCCESS);
					}

				}
				//printf("CHILD %d scrive in posizione %d \n", i, index+1);
				file_map[index] = 'A' + (char) i;

				if (sem_post(mutex) == -1) {
					perror("sem_post");
					exit(EXIT_FAILURE);
				}
			}
			break;
		case -1:
			perror("fork()");
			exit(EXIT_FAILURE);
		default:
			;
		}
	}
	for (int i = 0; i < FILE_SIZE + N; i++) {
		if (sem_post(proc_sem) == -1) {
			perror("sem_post");
			exit(EXIT_FAILURE);
		}
	}

	for (int j = 0; j < N; j++)
		wait(NULL);

	printf("Processo B completato BYE!\n");
	return;
}

int create_file_set_size(char *file_name, unsigned int file_size) {
	int res;
	int fd = open(file_name,
	O_CREAT | O_TRUNC | O_RDWR,
	S_IRUSR | S_IWUSR // l'utente proprietario del file avrà i permessi di lettura e scrittura sul nuovo file
	);

	CHECK_ERR(fd, "open file")

	res = ftruncate(fd, file_size);
	CHECK_ERR(res, "ftruncate()")

	return fd;
}

void read_write_in_file(int fd, int i) {

	char append_message = 'A' + (char) i;
	char tmp;
	int res;

	if (sem_wait(mutex) == -1) {
		perror("sem_wait");
		exit(EXIT_FAILURE);
	}

	while ((res = read(fd, &tmp, 1)) > 0 && tmp != 0)
		;
	if (res <= 0) {
		if (sem_post(mutex) == -1) {
			perror("sem_post");
			exit(EXIT_FAILURE);
		}
		//printf("CHILD numero %d EXIT_SUCCESS\n", i);
		exit(EXIT_SUCCESS);
	}
	res = lseek(fd, -1, SEEK_CUR);
	CHECK_ERR(res, "Lseek");

	printf("CHILD %d scrive a offset %d\n", i, res);
	res = write(fd, &append_message, sizeof(append_message));
	CHECK_ERR(res, "Write error")

	if (sem_post(mutex) == -1) {
		perror("sem_post");
		exit(EXIT_FAILURE);
	}

	return;
}

