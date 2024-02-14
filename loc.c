typedef char      int8;
typedef short     int16;
typedef int       int32;
typedef long long int64;

typedef unsigned char      uint8;
typedef unsigned short     uint16;
typedef unsigned int       uint32;
typedef unsigned long long uint64;

typedef _Bool bool;

enum { false = 0, true = 1 };

extern int64 system_call(int64 rax, int64 rsi, int64 rdi, int64 rdx, int64 r10, int64 r8, int64 r9);

int64 string_length(char* str) {
	char* p = str;
	for (; *p; p++);
	return p-str;
}

#define STDIN 0
#define STDOUT 1
#define STDERR 2

enum Linux_Open {
	OPEN_READ_ONLY   = 0x0,
	OPEN_WRITE_ONLY  = 0x1,
	OPEN_READ_WRITE  = 0x2,
	OPEN_CREATE      = 0x40,  // With an E!
	OPEN_EXCLUSIVE   = 0x80,  // Error if we tried creating a file and it already exists.
	OPEN_NO_TTY      = 0x100, // Prevent obtaining a terminal handle (e.g. Ctrl+c stuff)
	OPEN_TRUNCATE    = 0x200,
	OPEN_APPEND      = 0x400,
	OPEN_NON_BLOCK   = 0x800,
	OPEN_DIRECTORY   = 0x10000,
	OPEN_AUTO_CLOSE  = 0x80000, // @RemoveMe?
};

int32 file_open(char* path, int64 flags) {
	return system_call(2, (int64)path, flags, 0, 0, 0, 0);
}

void file_close(int32 file) {
	system_call(3, file, 0, 0, 0, 0, 0);
}

int64 file_read(int32 file, char* buffer, int64 length) {
	return system_call(0, (int64)buffer, length, 0, 0, 0, 0);
}

void file_write(int32 file, char* p, uint64 length) {
	system_call(1, file, (int64)p, length, 0, 0, 0);
}

void print_str(char* s) {
	file_write(STDOUT, s, string_length(s));
}

typedef struct DirectoryEntry64 {
	uint64  inode;
	int64  offset;
	uint16  length;
	uint8   type;
	char name[];
} DirectoryEntry64;

bool compare(char* p0, char* p1, uint64 length) {
	for (uint64 i = 0; i < length; i++) {
		if (p0[i] != p1[i])
			return false;
	}

	return true;
}

char* find_extension(char* str, int64 len) {
	for (char* p = str+len-2; p > str; p--) {
		if (*p == '.')
			return p;
	}

	return str+len;
}

int64 get_file_size(int32 file) {
	struct Status {
		uint64 dev;
		uint64 ino;
		uint64 nlink;

		uint32 mode;
		uint32 uid;
		uint32 gid;
		uint32 padding0;

		uint64 rdev;
		int64  size;
		int64  block_size;
		int64  blocks;

		uint64 atime;
		uint64 atime_nsec;
		uint64 mtime;
		uint64 mtime_nsec;
		uint64 ctime;
		uint64 ctime_nsec;

		int64 padding1[3];
	} status;

	system_call(5, file, (int64)&status, 0, 0, 0, 0);
	return status.size;
}
bool is_code_file(char* path, int64 path_length) {
	#define EXT(s) { s, sizeof(s)-1 }

	struct Extension {
		char* s;
		int64 length;
	};

	static const struct Extension extension_table[] = {
		EXT("asm"),
		EXT("c"),
		EXT("cc"),
		EXT("cpp"),
		EXT("cxx"),
		EXT("h"),
		EXT("hh"),
		EXT("hpp"),
		EXT("hxx"),
	};

	static const uint64 extension_table_length = sizeof(extension_table)/sizeof(*extension_table);

	char* path_ext        = find_extension(path, path_length)+1;
	int64 path_ext_length = (path+path_length)-path_ext;

	if (!path_ext_length)
		return false;

	for (const struct Extension* ext = extension_table; ext < extension_table+extension_table_length; ext++) {
		if (path_ext_length != ext->length)
			continue;

		if (!compare(ext->s, path_ext, path_ext_length))
			continue;

		return true;
	}

	return false;
}

char* load_file_virtual(int32 file, uint64 size) {
	char* p = (char*)system_call(9, 0, (size+4095)&-4096, 1, 0x02, file, 0);
	return p;
}

#define MAX_BASE10_DIGITS 20

struct DigitizeResult { char* str; int32 count; };

void digitize(uint64 n, char buffer[MAX_BASE10_DIGITS], char** out_str, int64* out_count) {
	int32 digit_count = 0;
	do buffer[MAX_BASE10_DIGITS-1-digit_count++] = 48 + (n % 10); while(n /= 10);
	*out_str   = buffer+MAX_BASE10_DIGITS-digit_count;
	*out_count = digit_count;
}

void print_int(uint64 n) {
	char s[MAX_BASE10_DIGITS];
	char* begin;
	int64 digit_count;
	digitize(n, s, &begin, &digit_count);
	file_write(STDOUT, begin, digit_count);
}

uint64 count_loc(char* s, uint64 size) {
	uint64 counter = 0;
	for (uint64 i = 0; i < size; i++) {
		char c = s[i];
		if (c == '\n')
			counter++;
	}

	return counter;
}

int main(int argc, char* args[]) {
	int32 fdir = file_open(".", OPEN_READ_ONLY | OPEN_DIRECTORY);

	if (fdir == -1) {
		print_str("ERROR: Couldn't open cwd.\n");
		return 1;
	}

	#define BUFFER_SIZE 8192
	char buffer[BUFFER_SIZE] = { 0 };
	char* p;

	uint64 total_loc = 0;

	while (true) {
		p = buffer;
		int64 read = system_call(217, fdir, (int64)buffer, BUFFER_SIZE, 0, 0, 0);

		if (!read)
			break;

		if (read == -1) {
			print_str("ERROR: Couldn't read dirs.\n");
			return 1;
		}

		while (p < buffer+read) {
			DirectoryEntry64* entry = (DirectoryEntry64*)p;
			p += entry->length;

			int64 namelen = string_length(entry->name);

			if (!is_code_file(entry->name, namelen))
				continue;

			// print_str("file: '");
			// print_str(entry->name);
			// print_str("'");

			int32 file = file_open(entry->name, OPEN_READ_ONLY);
			int64 file_size = get_file_size(file);
			char* file_data = load_file_virtual(file, file_size);
			uint64 loc = count_loc(file_data, file_size);
			total_loc += loc;

			// print_str(", loc: ");
			// print_int(loc);
			// print_str("\n");
		}
	}

	// print_str("Directory loc: ");
	print_int(total_loc);
	print_str("\n");

	return 0;
}

