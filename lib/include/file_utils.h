
#ifndef FILE_UTILS_H
#define FILE_UTILS_H

char *read_file(char *filename);
size_t num_lines(char *text);
void dump_buffer_to_file(char *filename, char *buffer, char *mode);


#endif
