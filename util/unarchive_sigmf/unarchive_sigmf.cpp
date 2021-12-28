#include <filesystem>
#include <iostream>
#include <vector>

#include "archive.h"
#include "archive_entry.h"

#ifdef _MSC_VER
# include <io.h>
# include <fcntl.h>
#else
# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>
#endif

const std::string SIGMF_EXT = ".sigmf";
const int DATA_BUFFER_SIZE = 10240;

void printUsage()
{
    std::cout << "Usage:  unarchive_sigmf <sigmf archive file name>" << std::endl;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        printUsage();
        return -1;
    }

    std::string in_filename(argv[1]);
    std::filesystem::path in_path(in_filename);
    if (in_path.extension() != SIGMF_EXT) {
        std::cerr << "argument did not end in " << SIGMF_EXT << std::endl;
        return -1;
    }

    std::filesystem::path in_core = in_path.stem();

    struct archive* archie;
    struct archive_entry* archie_ent;

    archie = archive_read_new();
    //archive_read_support_filter_tar(archie);
    archive_read_support_format_tar(archie);  // archive_read_support_format_tar() would have enabled many formats we don't need
    if (archive_read_open_filename(archie, in_filename.c_str(), DATA_BUFFER_SIZE) != ARCHIVE_OK) {
        std::cerr << "Failed to open archive: " << archive_error_string(archie) << std::endl;
        return -1;
    }

    // Now that we've opened in_filename with the [possibly-]relative path provided on the command line,
    // change the process's working directory to the new one we're going to extract into.
    if (!std::filesystem::create_directory(in_core)) {
        std::cerr << "Failed to create directory \"" << in_core << "\" (it may already exist)" << std::endl;
        return -1;
    }
    std::filesystem::current_path(in_core);

    std::vector<char> buf(DATA_BUFFER_SIZE);

    while (true) {
        int r = archive_read_next_header(archie, &archie_ent);
        if (r == ARCHIVE_EOF) {
            break;
        }
        if (r != ARCHIVE_OK) {
            std::cerr << "Failed archive_read_next_header(): " << archive_error_string(archie) << std::endl;
            break;
        }
        std::string entry_name(archive_entry_pathname(archie_ent));
        std::filesystem::path entry_path(entry_name);
        std::filesystem::create_directories(entry_path.parent_path());  // this will fail (return false) when called repeatedly with same arg; rely on write() to detect errors instead

        int ent_fid = open(entry_name.c_str(), O_WRONLY | O_CREAT
#ifndef _MSC_VER
            , S_IRUSR | S_IWUSR  // TODO should improve permissions
#endif  //unix
        );

        std::cout << "Writing " << (in_core / entry_path) << " ...";
        std::cout.flush();

        while (true) {
            la_ssize_t num_read = archive_read_data(archie, buf.data(), DATA_BUFFER_SIZE); 
            if (num_read <= 0) {
                break;
            }
            write(ent_fid, buf.data(), num_read);
        }

        close(ent_fid);

        std::cout << " done." << std::endl;
    }
    if (archive_read_free(archie) != ARCHIVE_OK) {
        std::cerr << "Failed to close archive: " << archive_error_string(archie) << std::endl;
        return -1;
    }

    return 0;
}
