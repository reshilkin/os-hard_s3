#include <string>
#include <cstring>
#include <iostream>
#include <fstream>
#include <array>
#include <vector>

struct section_header {
    uint32_t virtual_size, virtual_address, pointer_to_raw_data;
};

uint32_t get_pe_header_addr(std::ifstream &input) {
    uint32_t res;
    input.seekg(0x3C);
    input.read(reinterpret_cast<char *>(&res), 4);
    return res;
}

section_header read_header(std::ifstream &input) {
    uint32_t size, address, raw;
    input.seekg(8, std::ios_base::cur);
    input.read(reinterpret_cast<char *>(&size), 4);
    input.read(reinterpret_cast<char *>(&address), 4);
    input.seekg(4, std::ios_base::cur);
    input.read(reinterpret_cast<char *>(&raw), 4);
    input.seekg(16, std::ios_base::cur);
    return {size, address, raw};
}

uint32_t find_raw(uint32_t rva, std::ifstream &input) {
    input.seekg(get_pe_header_addr(input) + 0x108);
    section_header sh{};
    do {
        sh = read_header(input);
    } while (rva < sh.virtual_address || rva >= sh.virtual_address + sh.virtual_size);
    return sh.pointer_to_raw_data + rva - sh.virtual_address;
}

int is_pe(std::ifstream &input) {
    std::array<char, 4> buf = {0, 0, 0, 0};
    input.seekg(get_pe_header_addr(input));
    input.read(&buf[0], 4);
    if (buf == std::array<char, 4>({'P', 'E', '\0', '\0'})) {
        std::cout << "PE\n";
        return 0;
    } else {
        std::cout << "Not PE\n";
        return 1;
    }
}

void import_functions(std::ifstream &input) {
    uint32_t import_table_rva;
    uint32_t dll_name_rva;
    uint32_t import_lookup_rva;
    uint32_t function_name_rva;
    std::string tmp;

    input.seekg(get_pe_header_addr(input) + 0x90);
    input.read(reinterpret_cast<char *>(&import_table_rva), 4);
    for (uint32_t i = find_raw(import_table_rva, input);; i += 20) {
        input.seekg(i);
        input.read(reinterpret_cast<char *>(&import_lookup_rva), 4);
        input.seekg(i + 0xC);
        input.read(reinterpret_cast<char *>(&dll_name_rva), 4);
        if (dll_name_rva == 0)
            break;
        input.seekg(find_raw(dll_name_rva, input));
        std::getline(input, tmp, static_cast<char>(0));
        std::cout << tmp << '\n';
        for (uint32_t j = find_raw(import_lookup_rva, input);; j += 8) {
            input.seekg(j);
            input.read(reinterpret_cast<char *>(&function_name_rva), 4);
            if (function_name_rva == 0)
                break;
            function_name_rva &= ~(1 << 31);
            input.seekg(find_raw(function_name_rva, input) + 2);
            std::getline(input, tmp, static_cast<char>(0));
            std::cout << "    " << tmp << '\n';
        }
    }
}

void export_functions(std::ifstream &input) {
    uint32_t export_table_rva;
    uint32_t name_pointer_rva;
    uint32_t function_name_rva;
    uint32_t cnt;
    std::string tmp;

    input.seekg(get_pe_header_addr(input) + 0x88);
    input.read(reinterpret_cast<char *>(&export_table_rva), 4);
    input.seekg(find_raw(export_table_rva, input) + 0x18);
    input.read(reinterpret_cast<char *>(&cnt), 4);
    input.seekg(4, std::ios_base::cur);
    input.read(reinterpret_cast<char *>(&name_pointer_rva), 4);

    for (uint32_t i = find_raw(name_pointer_rva, input); cnt > 0; i += 4, cnt--) {
        input.seekg(i);
        input.read(reinterpret_cast<char *>(&function_name_rva), 4);
        input.seekg(find_raw(function_name_rva, input));
        std::getline(input, tmp, static_cast<char>(0));
        std::cout << tmp << '\n';
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "Unsupported number of arguments\n";
        return -1;
    }
    std::ifstream input(argv[2], std::ios::binary | std::ios::in);
    if (input.fail()) {
        std::cerr << "Could not open file " << argv[2] << "\n";
        return -1;
    }
    if (strcmp(argv[1], "is-pe") == 0) {
        return is_pe(input);
    } else if (strcmp(argv[1], "import-functions") == 0) {
        import_functions(input);
    } else if (strcmp(argv[1], "export-functions") == 0) {
        export_functions(input);
    } else {
        std::cerr << "Unsupported command\n";
        return -1;
    }
    return 0;
}