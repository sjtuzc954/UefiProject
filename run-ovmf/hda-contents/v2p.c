#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>

#define PAGEMAP_ENTRY 8
#define GET_BIT(X,Y) ((X & ((uint64_t)1<<Y)) >> Y)
#define GET_PFN(X) (X & 0x7FFFFFFFFFFFFF)

#define VAR_PATH "/sys/firmware/efi/efivars/MyVariable-a3a56e56-1d23-06dc-24bf-1473ff54e629"

int data_size;
char *end;
uint64_t phys_addr;
char *data_content;

enum CMD_TYPE {
    CMD_READ,
    CMD_WRITE
} cmd_type;

uint64_t get_phys_addr(uint64_t virt_addr, pid_t pid) {
    char pagemap_file[64];
    snprintf(pagemap_file, sizeof(pagemap_file), "/proc/%d/pagemap", pid);
    int fd = open(pagemap_file, O_RDONLY);
    if (fd < 0) {
        perror("open pagemap failed");
        exit(EXIT_FAILURE);
    }

    uint64_t page_size = getpagesize();
    uint64_t page_idx = virt_addr / page_size;
    uint64_t pte;
    size_t read_bytes = pread(fd, &pte, sizeof(uint64_t), page_idx * sizeof(uint64_t));
    if (read_bytes != sizeof(uint64_t)) {
        perror("pread failed");
        close(fd);
        exit(EXIT_FAILURE);
    }
    close(fd);
    
    if (!GET_BIT(pte, 63)) {
        perror("invalid pte");
        exit(EXIT_FAILURE);
    }
    uint64_t pfn = GET_PFN(pte);
    return (pfn * page_size) + (virt_addr % page_size);
}

void str2bytearray(const char *input, uint8_t *output, size_t *size) {
    char *token;
    char *inputCopy = strdup(input);
    size_t i = 0;

    token = strtok(inputCopy, "-");
    while (token != NULL) {
        output[i] = (uint8_t)strtol(token, NULL, 16);
        i++;
        token = strtok(NULL, "-");
    }

    *size = i;
    free(inputCopy);
}

void write_var(enum CMD_TYPE type) {
    printf("INFO: ready to write in var\n\tparameterID: %d\n\ttarget phys addr: 0x%lx\n\tdata_size: %d\n", type, phys_addr, data_size);
    if (type == 1) {
        printf("INFO: write data: %s\n", data_content);
    }
    
    uint8_t cc[4 + sizeof(uint64_t) + sizeof(uint64_t) + sizeof(uint64_t) + 128] = {
        0x07, 0x00, 0x00, 0x00
    };

    uint64_t type_64 = (uint64_t) type;
    memcpy(cc + 4, &type_64, sizeof(uint64_t));

    memcpy(cc + 4 + sizeof(uint64_t), &phys_addr, sizeof(uint64_t));

    uint64_t data_size_64 = (uint64_t) data_size;
    memcpy(cc + 4 + sizeof(uint64_t) + sizeof(uint64_t), &data_size_64, sizeof(uint64_t));

    size_t array_size = 0;
    if (type)
        str2bytearray(data_content, cc + 4 + 3*sizeof(uint64_t), &array_size);

    int need_write_size = type ? (4 + sizeof(uint64_t) + sizeof(uint64_t) + sizeof(uint64_t) + data_size) : (4 + sizeof(uint64_t) + sizeof(uint64_t) + sizeof(uint64_t));
    printf("INFO: cc array contents:\n");
    for (size_t i = 0; i < need_write_size; ++i) {
        printf("%02X ", cc[i]);
    }
    printf("\n");

    int fd = open(VAR_PATH, O_WRONLY | O_CREAT);
    if (fd == -1) {
        perror("ERROR: opening file");
        return;
    }

    ssize_t bytes_written = write(fd, cc, need_write_size);
    printf("INFO: bytes_written: %ld\n", bytes_written);
    if (bytes_written != need_write_size) {
        perror("ERROR: writing to file");
        close(fd);
        return;
    }

    close(fd);

    printf("INFO: successfully wrote %ld bytes to %s\n", bytes_written, VAR_PATH);

    return;
}

int main(int argc, char ** argv){
    printf("%d\n", argc);
    if(argc != 6 && argc != 5){
        printf("Usage:\n    v2p PID VIRTUAL_ADDRESS OPTION(0:READ/1:WRITE) DATA_SIZE [DATA_CONTENT]\n");
        return -1;
    }

    pid_t pid = (pid_t)strtol(argv[1], &end, 10);
    if (end == argv[1] || *end != '\0' || pid <= 0){
        printf("ERROR: invalid PID\n");
        return -1;
    }
    
    uint64_t virt_addr = strtoll(argv[2], NULL, 16);

    cmd_type = strtol(argv[3], NULL, 10);

    data_size = strtol(argv[4], NULL, 10);

    data_content = argv[5];

    phys_addr = get_phys_addr(virt_addr, pid);
    printf("Phys addr: %lu", phys_addr);

    write_var(cmd_type);

    return 0;
}
