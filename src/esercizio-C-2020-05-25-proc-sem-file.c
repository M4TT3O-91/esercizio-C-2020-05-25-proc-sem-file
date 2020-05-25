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
void write_in_file(int *fd, int i, off_t file_offset);

#define CHECK_ERR(a,msg) {if ((a) == -1) { perror((msg)); exit(EXIT_FAILURE); } }
#define CHECK_ERR_MMAP(a,msg) {if ((a) == MAP_FAILED) { perror((msg)); exit(EXIT_FAILURE); } }

int main(int argc, char *argv[]) {

	int res;

	proc_sem = mmap(NULL, // NULL: è il kernel a scegliere l'indirizzo
			sizeof(sem_t), // dimensione della memory map
			PROT_READ | PROT_WRITE, // memory map leggibile e scrivibile
			MAP_SHARED | MAP_ANONYMOUS, // memory map condivisibile con altri processi e senza file di appoggio
			-1, 0); // offset nel file
	CHECK_ERR_MMAP(proc_sem, "mmap")

	mutex = mmap(NULL, // NULL: è il kernel a scegliere l'indirizzo
			sizeof(sem_t), // dimensione della memory map
			PROT_READ | PROT_WRITE, // memory map leggibile e scrivibile
			MAP_SHARED | MAP_ANONYMOUS, // memory map condivisibile con altri processi e senza file di appoggio
			-1, 0); // offset nel file
	CHECK_ERR_MMAP(mutex, "mmap");

	res = sem_init(proc_sem, 1, // 1 => il semaforo è condiviso tra processi, 0 => il semaforo è condiviso tra threads del processo
			0 // valore iniziale del semaforo
			);
	if (res == -1) {
		perror("sem init");
		exit(EXIT_FAILURE);
	}
	res = sem_init(mutex, 1, // 1 => il semaforo è condiviso tra processi, 0 => il semaforo è condiviso tra threads del processo
			1 // valore iniziale del semaforo (se mettiamo 0 che succede?)
			);
	if (res == -1) {
		perror("sem init");
		exit(EXIT_FAILURE);
	}

	printf("ora avvio la soluzione_A()...\n");
	soluzione_A();

	printf("ed ora avvio la soluzione_B()...\n");
	//soluzione_B();

	printf("bye!\n");
	return 0;
}

void soluzione_A() {
//	usare le system call open(), lseek(), write()
	char *file_name = "output_A.txt";
	unsigned int file_size = FILE_SIZE;
	int fd = create_file_set_size(file_name, file_size);
	pid_t pid;
	int res;
	char *tmp;

	tmp = malloc(FILE_SIZE);

	for (int i = 0; i < N; i++) {
		pid = fork();
		switch (pid) {
		case 0:

			if ((res = read(fd, tmp, FILE_SIZE)) == -1) {
				perror("read error");
				exit(EXIT_FAILURE);
			}

			for (int k = 0; k < FILE_SIZE; k++) {
				if (sem_wait(proc_sem) == -1) {
					perror("sem_wait");
					exit(EXIT_FAILURE);
				}

				if (tmp[k] == 0)
					write_in_file(&fd, i, k);
				if (lseek(fd, 0, SEEK_CUR) == -1) {
					perror("lseek");
					exit(EXIT_FAILURE);
							}
			}
			exit(EXIT_SUCCESS);
		case -1:
			perror("fork()");
			exit(EXIT_FAILURE);
		default:
			for (int i = 0; i < FILE_SIZE + N; i++) {
				if (sem_post(proc_sem) == -1) {
					perror("sem_post");
					exit(EXIT_FAILURE);
				}
			}

			if (wait(NULL) == -1) {
				perror("wait");
				exit(EXIT_FAILURE);
			}
		}
	}
	printf("Processo completato BYE!\n");
	return;
}

//------------------------------------------------//

int create_file_set_size(char *file_name, unsigned int file_size) {
	// tratto da man 2 open
	// O_CREAT  If pathname does not exist, create it as a regular file.
	// O_TRUNC  If the file already exists and is a regular file and the access mode allows writing ... it will be truncated to length 0.
	// O_RDONLY, O_WRONLY, or O_RDWR  These request opening the file read-only, write-only, or read/write, respectively.

	int res;
	int fd = open(file_name,
	O_CREAT | O_RDWR,
	S_IRUSR | S_IWUSR);

	if (fd == -1) { // errore!
		perror("open()");
		return -1;
	}

	res = ftruncate(fd, 0);
	if (res == -1) {
		perror("ftruncate()");
		return -1;
	}

	res = ftruncate(fd, file_size);
	if (res == -1) {
		perror("ftruncate()");
		return -1;
	}

	return fd;
}

void write_in_file(int *fd, int i, off_t file_offset) {

	char append_message = 'A' + (char) i;

	if (lseek(*fd, file_offset, SEEK_SET) == -1) {
		perror("lseek");
		exit(EXIT_FAILURE);
	}

	// 3.4.2 Mutual exclusion solution, pag. 19
	if (sem_wait(mutex) == -1) {
		perror("sem_wait");
		exit(EXIT_FAILURE);
	}
	printf("Scrittura\n");
	// posizioniamo l'offset in fondo al file
	if ((lseek(*fd, file_offset, SEEK_SET)) == -1) {
		perror("lseek()");
		exit(EXIT_FAILURE);
	}

	int res = write(*fd, &append_message, sizeof(append_message));

	if (res == -1) {
		perror("write()");
		exit(EXIT_FAILURE);
	}
	if (sem_post(mutex) == -1) {
		perror("sem_post");
		exit(EXIT_FAILURE);
	}
	return;
}
