#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libelf.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <inttypes.h>
#include <gelf.h>
#include <stdarg.h>
#include <getopt.h>
#include <ctype.h>
#include <capstone/capstone.h>
#include <time.h> 

// #include "inject.h"

// ==============================================================================================================================

size_t len_section(Elf64_Ehdr *ptr, Elf64_Shdr *buffer_mdata_sh[], const char *section);

off_t search_section_name(char *sh_name_buffer[], Elf64_Ehdr *ptr, Elf64_Shdr *buffer_mdata_sh[], const char *section, size_t *len_sec);

int patch_target(void *p_entry, long pattern, int size, long patch);

int parse_phdr(Elf64_Ehdr *ptr, Elf64_Phdr *buffer_mdata_ph[]);

int parse_shdr(Elf64_Ehdr *ptr, Elf64_Shdr *buffer_mdata_sh[]);

uint64_t search_base_addr(Elf64_Phdr *buffer_mdata_phdr[], Elf64_Ehdr *ptr);

char  *parse_sh_name(Elf64_Ehdr *ptr, Elf64_Shdr *buffer_mdata_sh[], char *sh_name_buffer[]);

int x_pack_text(unsigned char *base_addr, size_t len_text, int random_int);

int r_pack_text(unsigned char *base_addr, size_t len_text, int random_int);

off_t search_section(const char *section, Elf64_Shdr *buffer_mdata_sh[], Elf64_Ehdr *ptr, int *i_sec);

int xor_encrypt(char *target_file);

int has_pie_or_not(Elf64_Phdr *buffer_mdata_ph[], Elf64_Ehdr *ptr);

int xor_encrypt_pie(char *file_ptr);

int not_encrypt(char *target_file);

int not_encrypt_pie(char *target_file);

// ==============================================================================================================================


int main(int argc, char **argv){

	if (argc == 3 && !strcmp(argv[2], "xor"))
	{
		xor_encrypt(argv[1]);
	}

	else if (argc == 3 && !strcmp(argv[2], "not"))
	{
		not_encrypt(argv[1]);
	}
	
	else if (argc != 2)
	{
		printf("Usage : <%s> <elf_target> <stub>\n", argv[0]);
	}

	return 0;
}


// ==============================================================================================================================

int patch_target(void *p_entry, long pattern, int size, long patch) {
	p_entry = (unsigned char *)p_entry;
	int result;

	for(long i = 0 ; i < size; i++)
	{
		result = *((long*)(p_entry+i)) ^ pattern;

		if(result == 0)
		{
			*((long*)(p_entry+i)) = patch;
			return 0;
		}
	}
	return -1;
}

// ==============================================================================================================================

off_t search_section_name(char *sh_name_buffer[], Elf64_Ehdr *ptr, Elf64_Shdr *buffer_mdata_sh[], const char *section, uint64_t *len_sec){

	for (size_t i = 0; i < ptr->e_shnum; i++)
	{
		if (!strcmp(sh_name_buffer[i], section))
		{
			*len_sec = buffer_mdata_sh[i]->sh_size;
			return buffer_mdata_sh[i]->sh_offset;
		}
		
	}
	
	return 1;
}

// ==============================================================================================================================

size_t len_section(Elf64_Ehdr *ptr, Elf64_Shdr *buffer_mdata_sh[], const char *section){

	size_t len=0;
	char *sh_name_buffer[ptr->e_shnum];

	parse_sh_name(ptr, buffer_mdata_sh, sh_name_buffer);

	for (size_t i = 0; i < ptr->e_shnum; i++)
	{
		if (!strcmp(sh_name_buffer[i], section))
		{
			len = buffer_mdata_sh[i]->sh_size;
		}
		
	}

	return len;
}

// ===========================================================================================================

uint64_t search_base_addr(Elf64_Phdr *buffer_mdata_phdr[], Elf64_Ehdr *ptr){

	int j = 0;
	uint64_t tab_addr[ptr->e_phnum];

	for (size_t i = 1; i < ptr->e_phnum; i++) {

		int type = buffer_mdata_phdr[i]->p_type;

		if (buffer_mdata_phdr[i]->p_type == PT_LOAD)
		{
			tab_addr[j]  = buffer_mdata_phdr[i]->p_vaddr;
			j++;
		}
	}

	int base_addr = tab_addr[0];

	for (size_t i = 1; i < j; i++)
	{
		if (tab_addr[i] < base_addr){
			base_addr = tab_addr[i];
		}
	}

	return base_addr;
}

// ===========================================================================================================

char  *parse_sh_name(Elf64_Ehdr *ptr, Elf64_Shdr *buffer_mdata_sh[], char *sh_name_buffer[]){

	Elf64_Shdr *shstrtab_header = (Elf64_Shdr *) ((char *)ptr + (ptr->e_shoff + ptr->e_shentsize * ptr->e_shstrndx));

	const char *shstrndx = (const char *)ptr + shstrtab_header->sh_offset;

	for (size_t i = 0; i < ptr->e_shnum; i++){

		sh_name_buffer[i] = (char *) (shstrndx + buffer_mdata_sh[i]->sh_name);

	}

	return 0;
}

// ===========================================================================================================

int parse_phdr(Elf64_Ehdr *ptr, Elf64_Phdr *buffer_mdata_ph[]){

	size_t number_of_sections = ptr->e_phnum;

	Elf64_Ehdr *ptr_2 = (Elf64_Ehdr *)ptr;

	for (size_t i = 0; i < ptr->e_phnum; i++)
	{
		// (char *) buffer_mdata_ph[i] = (Elf64_Phdr *)((char *)ptr + (ptr_2->e_phoff + ptr_2->e_phentsize * i));

		buffer_mdata_ph[i]  = (Elf64_Phdr *) ((char *)ptr + (ptr_2->e_phoff + ptr_2->e_phentsize * i));

		// buffer_mdata_ph[i] = (Elf64_Ehdr *)ph_ptr_tmp;
	}

	return 0;
}

// ===========================================================================================================

int parse_shdr(Elf64_Ehdr *ptr, Elf64_Shdr *buffer_mdata_sh[]){

	size_t number_of_sections = ptr->e_shnum;

	Elf64_Ehdr *ptr_2 = (Elf64_Ehdr *)ptr;

	for (size_t i = 0; i < ptr->e_shnum; i++)
	{
		// (char *) buffer_mdata_ph[i] = (Elf64_Phdr *)((char *)ptr + (ptr_2->e_phoff + ptr_2->e_phentsize * i));

		buffer_mdata_sh[i]  = (Elf64_Shdr *) ((char *)ptr + (ptr_2->e_shoff + ptr_2->e_shentsize * i));

		// buffer_mdata_ph[i] = (Elf64_Ehdr *)ph_ptr_tmp;
	}

	return 0;
}

// ===========================================================================================================

int x_pack_text(unsigned char *base_addr, size_t len_text, int random_int){

	for (size_t i = 0; i < len_text; i++)
	{
		base_addr[i] ^= random_int;
	}
	
	return 0;
}

// ===========================================================================================================

off_t search_section(const char *section, Elf64_Shdr *buffer_mdata_sh[], Elf64_Ehdr *ptr, int *i_sec){

	off_t offset = 0;
	Elf64_Shdr *shstrtab_header;

	char *sh_name_buffer[ptr->e_shnum];

	shstrtab_header = (Elf64_Shdr *) ((char *)ptr + (ptr->e_shoff + ptr->e_shentsize * ptr->e_shstrndx));

	const char *shstrndx = (const char *)ptr + shstrtab_header->sh_offset;

	for (size_t i = 0; i < ptr->e_shnum; i++){

		sh_name_buffer[i] = (char *)shstrndx + buffer_mdata_sh[i]->sh_name;

	}

	for (size_t i = 0; i < ptr->e_shnum; i++)
	{
		if (strcmp(sh_name_buffer[i], section) == 0){
			offset = buffer_mdata_sh[i]->sh_offset;
			*i_sec = i;
			return offset;
		}

	}

	return -1;
}

// ===========================================================================================================

int xor_encrypt(char *target_file){
	int fd=0;
	int fd_stub=0;
	struct stat stat_file;
	struct stat stat_stub;
	unsigned char *file_ptr;
	unsigned char *ptr_stub;
	uint64_t len_sec;
	size_t len_text;
	off_t txt_offset;
	size_t len_txt_seg=0;

	srand(time(NULL)); 
	int random_int = 1 + rand() % (255 - 1 + 1);

	printf("RandomInt : %d\n", random_int);
	
	fd = open(target_file, O_RDWR);

	char *path_stub = "stub_xor";

	fd_stub = open(path_stub, O_RDWR);

	if (fd == -1 || fd_stub == -1)
	{
		perror("Open has failed\n");
	}

	if (fstat(fd, &stat_file) != 0 || fstat(fd_stub, &stat_stub) != 0){
		printf("[ERROR] fstat failed\n");
		exit(-1);
	}

	file_ptr = mmap(NULL, stat_file.st_size, PROT_WRITE | PROT_READ | PROT_EXEC, MAP_SHARED, fd, 0);

	ptr_stub = (unsigned char *)mmap(NULL, stat_stub.st_size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_SHARED, fd_stub, 0);

	Elf64_Ehdr *s_ptr = (Elf64_Ehdr *)ptr_stub;

	Elf64_Ehdr *ptr = (Elf64_Ehdr *)file_ptr;

	Elf64_Shdr *buffer_mdata_sh[ptr->e_shnum];
	Elf64_Shdr *buffer_mdata_sh_stub[s_ptr->e_shnum];

	Elf64_Phdr *buffer_mdata_ph[ptr->e_phnum];
	Elf64_Phdr *buffer_mdata_ph_stub[s_ptr->e_phnum];

	size_t len_stub = stat_stub.st_size;
	unsigned long txt_end=0;

	char *sh_name_buffer[s_ptr->e_shnum];
	char *v_sh_name_buffer[ptr->e_shnum];

	parse_phdr(ptr, buffer_mdata_ph);
	parse_phdr(s_ptr, buffer_mdata_ph_stub);

	if (!search_base_addr(buffer_mdata_ph, ptr))
	{
		printf("This binary has the pie !\n");

		close(fd);
		close(fd_stub);

		if (munmap(file_ptr, stat_file.st_size) != 0||munmap(ptr_stub, stat_stub.st_size) != 0){
			printf("[ERROR] munmap failed\n");
		}

		xor_encrypt_pie(target_file);

		return 0;
	}
	

	uint64_t base = search_base_addr(buffer_mdata_ph, ptr);

	parse_shdr(ptr, buffer_mdata_sh);
	parse_shdr(s_ptr, buffer_mdata_sh_stub);

	parse_sh_name(s_ptr, buffer_mdata_sh_stub, sh_name_buffer);
	parse_sh_name(ptr, buffer_mdata_sh, v_sh_name_buffer);

	size_t codecave = stat_stub.st_size;

	for (size_t i = 0; i < ptr->e_phnum; i++)
	{
		if (buffer_mdata_ph[i]->p_type == PT_LOAD && buffer_mdata_ph[i]->p_flags == 0x5)
		{
			size_t len_load = buffer_mdata_ph[i]->p_memsz;

			len_text = len_section(ptr, buffer_mdata_sh, ".text");

			txt_offset = buffer_mdata_ph[i]->p_offset;

			len_txt_seg = buffer_mdata_ph[i]->p_filesz;

			txt_end = buffer_mdata_ph[i]->p_offset + buffer_mdata_ph[i]->p_filesz;

		}
		else
		{
			if (buffer_mdata_ph[i]->p_type == PT_LOAD && (buffer_mdata_ph[i]->p_offset - txt_end) < codecave)
			{
				codecave = buffer_mdata_ph[i]->p_offset - txt_end;
			}
		}	
	}

	off_t ptr_stub_text = search_section_name(sh_name_buffer, s_ptr, buffer_mdata_sh_stub, ".text", &len_sec);

	if (len_stub > codecave)
	{
		printf("Stub too big\n");
	}

	memmove(file_ptr + txt_end, ptr_stub + ptr_stub_text, len_sec);

	int i_sec__;

	off_t dot_txt = search_section(".text", buffer_mdata_sh, ptr, &i_sec__);

	size_t len_txt_stub = len_section((Elf64_Ehdr *)ptr_stub, buffer_mdata_sh_stub, ".text");

	size_t len_sec_txt=0;

	off_t ptr_text = search_section_name(v_sh_name_buffer, ptr, buffer_mdata_sh, ".text", &len_sec_txt);

	patch_target((unsigned char *)file_ptr + txt_end, 0x4444444444444444, len_txt_stub, (long)base + txt_offset);

	patch_target((unsigned char *)file_ptr + txt_end, 0x5555555555555555, len_txt_stub, (long)len_txt_seg);

	patch_target((unsigned char *)file_ptr + txt_end, 0x2222222222222222, len_txt_stub, (long)len_text);

	patch_target((unsigned char *)file_ptr + txt_end, 0x3333333333333333, len_txt_stub, (long)base + ptr_text);

	patch_target((unsigned char *)file_ptr + txt_end, 0x1111111111111111, len_txt_stub, (long)ptr->e_entry);

	patch_target((unsigned char *)file_ptr + txt_end, 0x6666666666666666, len_txt_stub, (long)random_int);

	x_pack_text(file_ptr + ptr_text, len_text, random_int);

	ptr->e_entry = (uint64_t)base + txt_end;

	if (munmap(file_ptr, stat_file.st_size) != 0||munmap(ptr_stub, stat_stub.st_size) != 0){
		printf("[ERROR] munmap failed\n");
		exit(-1);
	}

	close(fd);
	close(fd_stub);

	return 0;
}

// ===========================================================================================================

int has_pie_or_not(Elf64_Phdr *buffer_mdata_ph[], Elf64_Ehdr *ptr){

	for (size_t i = 0; i < ptr->e_phnum; i++)
	{
		if (buffer_mdata_ph[i]->p_type == PT_LOAD && buffer_mdata_ph[i]->p_flags == 0x5)
		{
			if (!buffer_mdata_ph[i]->p_vaddr)
			{
				return 0;
			}
			else
			{
				return 1;
			}
			
		}
		
	}

}

int xor_encrypt_pie(char *target_file){
	int fd=0;
	int fd_stub=0;
	struct stat stat_file;
	struct stat stat_stub;
	unsigned char *file_ptr;
	unsigned char *ptr_stub;
	uint64_t len_sec;
	size_t len_text;
	off_t txt_offset;
	size_t len_txt_seg=0;
	size_t len_load = 0;

	srand(time(NULL)); 
	int random_int = 1 + rand() % (255 - 1 + 1);

	printf("RandomInt : %d\n", random_int);
	
	fd = open(target_file, O_RDWR);

	char *path_stub = "stub_xor_pie";

	fd_stub = open(path_stub, O_RDWR);

	if (fd == -1 || fd_stub == -1)
	{
		perror("Open has failed\n");
	}

	if (fstat(fd, &stat_file) != 0 || fstat(fd_stub, &stat_stub) != 0){
		printf("[ERROR] fstat failed\n");
		exit(-1);
	}

	file_ptr = mmap(NULL, stat_file.st_size, PROT_WRITE | PROT_READ | PROT_EXEC, MAP_SHARED, fd, 0);

	ptr_stub = (unsigned char *)mmap(NULL, stat_stub.st_size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_SHARED, fd_stub, 0);

	Elf64_Ehdr *s_ptr = (Elf64_Ehdr *)ptr_stub;

	Elf64_Ehdr *ptr = (Elf64_Ehdr *)file_ptr;

	Elf64_Shdr *buffer_mdata_sh[ptr->e_shnum];
	Elf64_Shdr *buffer_mdata_sh_stub[s_ptr->e_shnum];

	Elf64_Phdr *buffer_mdata_ph[ptr->e_phnum];
	Elf64_Phdr *buffer_mdata_ph_stub[s_ptr->e_phnum];

	size_t len_stub = stat_stub.st_size;
	unsigned long txt_end=0;

	char *sh_name_buffer[s_ptr->e_shnum];
	char *v_sh_name_buffer[ptr->e_shnum];

	parse_phdr(ptr, buffer_mdata_ph);
	parse_phdr(s_ptr, buffer_mdata_ph_stub);

	uint64_t base = search_base_addr(buffer_mdata_ph, ptr);

	parse_shdr(ptr, buffer_mdata_sh);
	parse_shdr(s_ptr, buffer_mdata_sh_stub);

	parse_sh_name(s_ptr, buffer_mdata_sh_stub, sh_name_buffer);
	parse_sh_name(ptr, buffer_mdata_sh, v_sh_name_buffer);

	size_t codecave = stat_stub.st_size;

	for (size_t i = 0; i < ptr->e_phnum; i++)
	{
		if (buffer_mdata_ph[i]->p_type == PT_LOAD && buffer_mdata_ph[i]->p_flags == 0x5)
		{
			len_load = buffer_mdata_ph[i]->p_memsz;

			len_text = len_section(ptr, buffer_mdata_sh, ".text");

			txt_offset = buffer_mdata_ph[i]->p_offset;

			len_txt_seg = buffer_mdata_ph[i]->p_filesz;

			txt_end = buffer_mdata_ph[i]->p_offset + buffer_mdata_ph[i]->p_filesz;

		}
		else
		{
			if (buffer_mdata_ph[i]->p_type == PT_LOAD && (buffer_mdata_ph[i]->p_offset - txt_end) < codecave)
			{
				codecave = buffer_mdata_ph[i]->p_offset - txt_end;
			}
		}	
	}

	off_t ptr_stub_text = search_section_name(sh_name_buffer, s_ptr, buffer_mdata_sh_stub, ".text", &len_sec);

	if (len_stub > codecave)
	{
		printf("Stub too big\n");
	}

	memmove(file_ptr + txt_end, ptr_stub + ptr_stub_text, len_sec);

	int i_sec__;

	off_t dot_txt = search_section(".text", buffer_mdata_sh, ptr, &i_sec__);

	size_t len_txt_stub = len_section((Elf64_Ehdr *)ptr_stub, buffer_mdata_sh_stub, ".text");

	size_t len_sec_txt=0;
	// long base_pie_load = 0x000055555555400;
	long base_pie = 0x0000555555554530;

	off_t ptr_text = search_section_name(v_sh_name_buffer, ptr, buffer_mdata_sh, ".text", &len_sec_txt);
	
	// patch_target((unsigned char *)file_ptr + txt_end, 0x4444444444444444, len_txt_stub, (long)txt_offset);

	patch_target((unsigned char *)file_ptr + txt_end, 0x5555555555555555, len_txt_stub, (long)len_txt_seg);

	patch_target((unsigned char *)file_ptr + txt_end, 0x2222222222222222, len_txt_stub, (long)len_text);

	patch_target((unsigned char *)file_ptr + txt_end, 0x3333333333333333, len_txt_stub, (long)base + txt_end);

	patch_target((unsigned char *)file_ptr + txt_end, 0x1111111111111111, len_txt_stub, (long)ptr->e_entry);

	patch_target((unsigned char *)file_ptr + txt_end, 0x6666666666666666, len_txt_stub, (long)random_int);

	patch_target((unsigned char *)file_ptr + txt_end, 0x7777777777777777, len_txt_stub, (long)ptr_text);

	x_pack_text(file_ptr + ptr_text, len_text, random_int);

	ptr->e_entry = (uint64_t)base + txt_end;

	if (munmap(file_ptr, stat_file.st_size) != 0||munmap(ptr_stub, stat_stub.st_size) != 0){
		printf("[ERROR] munmap failed\n");
		exit(-1);
	}

	close(fd);
	close(fd_stub);

	return 0;
}

// ===========================================================================================================

int r_pack_text(unsigned char *base_addr, size_t len_text, int random_int){

	for (size_t i = 0; i < len_text; i++)
	{
		base_addr[i] = ~base_addr[i] ^ random_int;
	}
	

	return 0;
}

// ===========================================================================================================

int not_encrypt(char *target_file){
	int fd=0;
	int fd_stub=0;
	struct stat stat_file;
	struct stat stat_stub;
	unsigned char *file_ptr;
	unsigned char *ptr_stub;
	uint64_t len_sec;
	size_t len_text;
	off_t txt_offset;
	size_t len_txt_seg=0;

	srand(time(NULL)); 
	int random_int = 1 + rand() % (255 - 1 + 1);

	printf("RandomInt : %d\n", random_int);
	
	fd = open(target_file, O_RDWR);

	char *path_stub = "stub_not";

	fd_stub = open(path_stub, O_RDWR);

	if (fd == -1 || fd_stub == -1)
	{
		perror("Open has failed\n");
	}

	if (fstat(fd, &stat_file) != 0 || fstat(fd_stub, &stat_stub) != 0){
		printf("[ERROR] fstat failed\n");
		exit(-1);
	}

	file_ptr = mmap(NULL, stat_file.st_size, PROT_WRITE | PROT_READ | PROT_EXEC, MAP_SHARED, fd, 0);

	ptr_stub = (unsigned char *)mmap(NULL, stat_stub.st_size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_SHARED, fd_stub, 0);

	Elf64_Ehdr *s_ptr = (Elf64_Ehdr *)ptr_stub;

	Elf64_Ehdr *ptr = (Elf64_Ehdr *)file_ptr;

	Elf64_Shdr *buffer_mdata_sh[ptr->e_shnum];
	Elf64_Shdr *buffer_mdata_sh_stub[s_ptr->e_shnum];

	Elf64_Phdr *buffer_mdata_ph[ptr->e_phnum];
	Elf64_Phdr *buffer_mdata_ph_stub[s_ptr->e_phnum];

	size_t len_stub = stat_stub.st_size;
	unsigned long txt_end=0;

	char *sh_name_buffer[s_ptr->e_shnum];
	char *v_sh_name_buffer[ptr->e_shnum];

	parse_phdr(ptr, buffer_mdata_ph);
	parse_phdr(s_ptr, buffer_mdata_ph_stub);

	if (!search_base_addr(buffer_mdata_ph, ptr))
	{
		printf("This binary has the pie !\n");

		close(fd);
		close(fd_stub);

		if (munmap(file_ptr, stat_file.st_size) != 0||munmap(ptr_stub, stat_stub.st_size) != 0){
			printf("[ERROR] munmap failed\n");
		}

		not_encrypt_pie(target_file);

		return 0;
	}
	

	uint64_t base = search_base_addr(buffer_mdata_ph, ptr);

	parse_shdr(ptr, buffer_mdata_sh);
	parse_shdr(s_ptr, buffer_mdata_sh_stub);

	parse_sh_name(s_ptr, buffer_mdata_sh_stub, sh_name_buffer);
	parse_sh_name(ptr, buffer_mdata_sh, v_sh_name_buffer);

	size_t codecave = stat_stub.st_size;

	for (size_t i = 0; i < ptr->e_phnum; i++)
	{
		if (buffer_mdata_ph[i]->p_type == PT_LOAD && buffer_mdata_ph[i]->p_flags == 0x5)
		{
			size_t len_load = buffer_mdata_ph[i]->p_memsz;

			len_text = len_section(ptr, buffer_mdata_sh, ".text");

			txt_offset = buffer_mdata_ph[i]->p_offset;

			len_txt_seg = buffer_mdata_ph[i]->p_filesz;

			txt_end = buffer_mdata_ph[i]->p_offset + buffer_mdata_ph[i]->p_filesz;

		}
		else
		{
			if (buffer_mdata_ph[i]->p_type == PT_LOAD && (buffer_mdata_ph[i]->p_offset - txt_end) < codecave)
			{
				codecave = buffer_mdata_ph[i]->p_offset - txt_end;
			}
		}	
	}

	off_t ptr_stub_text = search_section_name(sh_name_buffer, s_ptr, buffer_mdata_sh_stub, ".text", &len_sec);

	if (len_stub > codecave)
	{
		printf("Stub too big\n");
	}

	memmove(file_ptr + txt_end, ptr_stub + ptr_stub_text, len_sec);

	int i_sec__;

	off_t dot_txt = search_section(".text", buffer_mdata_sh, ptr, &i_sec__);

	size_t len_txt_stub = len_section((Elf64_Ehdr *)ptr_stub, buffer_mdata_sh_stub, ".text");

	size_t len_sec_txt=0;

	off_t ptr_text = search_section_name(v_sh_name_buffer, ptr, buffer_mdata_sh, ".text", &len_sec_txt);

	patch_target((unsigned char *)file_ptr + txt_end, 0x4444444444444444, len_txt_stub, (long)base + txt_offset);

	patch_target((unsigned char *)file_ptr + txt_end, 0x5555555555555555, len_txt_stub, (long)len_txt_seg);

	patch_target((unsigned char *)file_ptr + txt_end, 0x2222222222222222, len_txt_stub, (long)len_text);

	patch_target((unsigned char *)file_ptr + txt_end, 0x3333333333333333, len_txt_stub, (long)base + ptr_text);

	patch_target((unsigned char *)file_ptr + txt_end, 0x1111111111111111, len_txt_stub, (long)ptr->e_entry);

	patch_target((unsigned char *)file_ptr + txt_end, 0x6666666666666666, len_txt_stub, (long)random_int);

	r_pack_text(file_ptr + ptr_text, len_text, random_int);

	ptr->e_entry = (uint64_t)base + txt_end;

	if (munmap(file_ptr, stat_file.st_size) != 0||munmap(ptr_stub, stat_stub.st_size) != 0){
		printf("[ERROR] munmap failed\n");
		exit(-1);
	}

	close(fd);
	close(fd_stub);

	return 0;
}

// ===========================================================================================================

int not_encrypt_pie(char *target_file){
	int fd=0;
	int fd_stub=0;
	struct stat stat_file;
	struct stat stat_stub;
	unsigned char *file_ptr;
	unsigned char *ptr_stub;
	uint64_t len_sec;
	size_t len_text;
	off_t txt_offset;
	size_t len_txt_seg=0;
	size_t len_load = 0;

	srand(time(NULL)); 
	int random_int = 1 + rand() % (255 - 1 + 1);

	printf("RandomInt : %d\n", random_int);
	
	fd = open(target_file, O_RDWR);

	char *path_stub = "stub_not_pie";

	fd_stub = open(path_stub, O_RDWR);

	if (fd == -1 || fd_stub == -1)
	{
		perror("Open has failed\n");
	}

	if (fstat(fd, &stat_file) != 0 || fstat(fd_stub, &stat_stub) != 0){
		printf("[ERROR] fstat failed\n");
		exit(-1);
	}

	file_ptr = mmap(NULL, stat_file.st_size, PROT_WRITE | PROT_READ | PROT_EXEC, MAP_SHARED, fd, 0);

	ptr_stub = (unsigned char *)mmap(NULL, stat_stub.st_size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_SHARED, fd_stub, 0);

	Elf64_Ehdr *s_ptr = (Elf64_Ehdr *)ptr_stub;

	Elf64_Ehdr *ptr = (Elf64_Ehdr *)file_ptr;

	Elf64_Shdr *buffer_mdata_sh[ptr->e_shnum];
	Elf64_Shdr *buffer_mdata_sh_stub[s_ptr->e_shnum];

	Elf64_Phdr *buffer_mdata_ph[ptr->e_phnum];
	Elf64_Phdr *buffer_mdata_ph_stub[s_ptr->e_phnum];

	size_t len_stub = stat_stub.st_size;
	unsigned long txt_end=0;

	char *sh_name_buffer[s_ptr->e_shnum];
	char *v_sh_name_buffer[ptr->e_shnum];

	parse_phdr(ptr, buffer_mdata_ph);
	parse_phdr(s_ptr, buffer_mdata_ph_stub);

	uint64_t base = search_base_addr(buffer_mdata_ph, ptr);

	parse_shdr(ptr, buffer_mdata_sh);
	parse_shdr(s_ptr, buffer_mdata_sh_stub);

	parse_sh_name(s_ptr, buffer_mdata_sh_stub, sh_name_buffer);
	parse_sh_name(ptr, buffer_mdata_sh, v_sh_name_buffer);

	size_t codecave = stat_stub.st_size;

	for (size_t i = 0; i < ptr->e_phnum; i++)
	{
		if (buffer_mdata_ph[i]->p_type == PT_LOAD && buffer_mdata_ph[i]->p_flags == 0x5)
		{
			len_load = buffer_mdata_ph[i]->p_memsz;

			len_text = len_section(ptr, buffer_mdata_sh, ".text");

			txt_offset = buffer_mdata_ph[i]->p_offset;

			len_txt_seg = buffer_mdata_ph[i]->p_filesz;

			txt_end = buffer_mdata_ph[i]->p_offset + buffer_mdata_ph[i]->p_filesz;

		}
		else
		{
			if (buffer_mdata_ph[i]->p_type == PT_LOAD && (buffer_mdata_ph[i]->p_offset - txt_end) < codecave)
			{
				codecave = buffer_mdata_ph[i]->p_offset - txt_end;
			}
		}	
	}

	off_t ptr_stub_text = search_section_name(sh_name_buffer, s_ptr, buffer_mdata_sh_stub, ".text", &len_sec);

	if (len_stub > codecave)
	{
		printf("Stub too big\n");
	}

	memmove(file_ptr + txt_end, ptr_stub + ptr_stub_text, len_sec);

	int i_sec__;

	off_t dot_txt = search_section(".text", buffer_mdata_sh, ptr, &i_sec__);

	size_t len_txt_stub = len_section((Elf64_Ehdr *)ptr_stub, buffer_mdata_sh_stub, ".text");

	size_t len_sec_txt=0;
	// long base_pie_load = 0x000055555555400;
	long base_pie = 0x0000555555554530;

	off_t ptr_text = search_section_name(v_sh_name_buffer, ptr, buffer_mdata_sh, ".text", &len_sec_txt);
	
	// patch_target((unsigned char *)file_ptr + txt_end, 0x4444444444444444, len_txt_stub, (long)txt_offset);

	patch_target((unsigned char *)file_ptr + txt_end, 0x5555555555555555, len_txt_stub, (long)len_txt_seg);

	patch_target((unsigned char *)file_ptr + txt_end, 0x2222222222222222, len_txt_stub, (long)len_text);

	patch_target((unsigned char *)file_ptr + txt_end, 0x3333333333333333, len_txt_stub, (long)base + txt_end);

	patch_target((unsigned char *)file_ptr + txt_end, 0x1111111111111111, len_txt_stub, (long)ptr->e_entry);

	patch_target((unsigned char *)file_ptr + txt_end, 0x6666666666666666, len_txt_stub, (long)random_int);

	patch_target((unsigned char *)file_ptr + txt_end, 0x7777777777777777, len_txt_stub, (long)ptr_text);

	r_pack_text(file_ptr + ptr_text, len_text, random_int);

	ptr->e_entry = (uint64_t)base + txt_end;

	if (munmap(file_ptr, stat_file.st_size) != 0||munmap(ptr_stub, stat_stub.st_size) != 0){
		printf("[ERROR] munmap failed\n");
		exit(-1);
	}

	close(fd);
	close(fd_stub);

	return 0;
}
