//Motasoft Area Compiler Collection (macc) 
//writen by : hossein Lotfaraghi
//Use free and Send Salavat (;

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/stat.h>

#define MACC_VERSION "0.1.0"
#define MAX_SOURCES 256
#define MAX_ARGS 1024

typedef struct {
    char *sources[MAX_SOURCES];
    int source_count;

    char *output;

    int optimize;
    int debug;
} MaccOptions;



static void print_version(void)
{
    printf("Motasoft Area Compiler Collection (MACC) %s\n",
           MACC_VERSION);
    printf("C# compiler driver for Linux\n");
}


static void print_help(void)
{
    printf(
        "Motasoft Area Compiler Collection (MACC)\n"
        "\n"
        "Usage:\n"
        "  macc [options] file.cs...\n"
        "\n"
        "Options:\n"
        "  -o <file>       Specify output executable\n"
        "  -O              Enable optimization\n"
        "  -O1             Enable optimization level 1\n"
        "  -O2             Enable optimization level 2\n"
        "  -O3             Enable optimization level 3\n"
        "  -g              Generate debug information\n"
        "  -h, --help      Display this help\n"
        "  -v, --version   Display compiler version\n"
        "\n"
        "Examples:\n"
        "  macc hello.cs -o hello\n"
        "  macc main.cs math.cs -o app\n"
        "  macc -O2 main.cs -o app\n"
        "  macc -g main.cs -o debug\n"
        "\n"
        "MACC = Motasoft Area Compiler Collection\n"
    );
}




static int parse_arguments(
    int argc,
    char **argv,
    MaccOptions *options)
{
    memset(options, 0, sizeof(MaccOptions));

    options->output = "a.out";

    for (int i = 1; i < argc; i++) {

        char *arg = argv[i];

        if (strcmp(arg, "-h") == 0 ||
            strcmp(arg, "--help") == 0) {

            print_help();
            exit(0);
        }

        if (strcmp(arg, "-v") == 0 ||
            strcmp(arg, "--version") == 0) {

            print_version();
            exit(0);
        }

        if (strcmp(arg, "-O") == 0 ||
            strcmp(arg, "-O1") == 0 ||
            strcmp(arg, "-O2") == 0 ||
            strcmp(arg, "-O3") == 0) {

            options->optimize = 1;
            continue;
        }

        if (strcmp(arg, "-g") == 0) {
            options->debug = 1;
            continue;
        }

        if (strcmp(arg, "-o") == 0) {

            if (i + 1 >= argc) {
                fprintf(stderr,
                        "macc: error: missing argument after '-o'\n");
                return 1;
            }

            options->output = argv[++i];
            continue;
        }

     
        size_t len = strlen(arg);

        if (len >= 3 &&
            strcmp(arg + len - 3, ".cs") == 0) {

            if (options->source_count >= MAX_SOURCES) {
                fprintf(stderr,
                        "macc: error: too many source files\n");
                return 1;
            }

            options->sources[
                options->source_count++
            ] = arg;

            continue;
        }

      
        if (arg[0] == '-') {
            fprintf(stderr,
                    "macc: error: unknown option '%s'\n",
                    arg);
            return 1;
        }

        fprintf(stderr,
                "macc: error: unsupported input '%s'\n",
                arg);

        return 1;
    }

    if (options->source_count == 0) {
        fprintf(stderr,
                "macc: error: no input files\n");
        return 1;
    }

    return 0;
}




static int check_sources(const MaccOptions *options)
{
    for (int i = 0; i < options->source_count; i++) {

        if (access(options->sources[i], R_OK) != 0) {

            fprintf(stderr,
                    "macc: error: cannot read '%s': %s\n",
                    options->sources[i],
                    strerror(errno));

            return 1;
        }
    }

    return 0;
}




static int execute(char **args)
{
    pid_t pid = fork();

    if (pid < 0) {
        perror("macc: fork");
        return 1;
    }

    if (pid == 0) {

        execvp(args[0], args);

        fprintf(stderr,
                "macc: error: cannot execute '%s': %s\n",
                args[0],
                strerror(errno));

        _exit(127);
    }

    int status;

    if (waitpid(pid, &status, 0) < 0) {
        perror("macc: waitpid");
        return 1;
    }

    if (WIFEXITED(status))
        return WEXITSTATUS(status);

    if (WIFSIGNALED(status)) {

        fprintf(stderr,
                "macc: compiler terminated by signal %d\n",
                WTERMSIG(status));

        return 128 + WTERMSIG(status);
    }

    return 1;
}




static int check_dotnet(void)
{
    if (access("/usr/bin/dotnet", X_OK) == 0)
        return 0;

    if (access("/usr/local/bin/dotnet", X_OK) == 0)
        return 0;

    /*
     * Let execvp search PATH.
     */
    return 0;
}




static int compile_project(const MaccOptions *options)
{
    if (check_dotnet() != 0)
        return 1;

    /*
     * Create a temporary project directory.
     */
    char template[] = "/tmp/macc-XXXXXX";

    char *directory = mkdtemp(template);

    if (!directory) {
        perror("macc: mkdtemp");
        return 1;
    }

    char project_file[4096];

    snprintf(
        project_file,
        sizeof(project_file),
        "%s/MaccProgram.csproj",
        directory
    );

    FILE *project = fopen(project_file, "w");

    if (!project) {
        perror("macc: create project");
        return 1;
    }

    fprintf(
        project,
        "<Project Sdk=\"Microsoft.NET.Sdk\">\n"
        "  <PropertyGroup>\n"
        "    <OutputType>Exe</OutputType>\n"
        "    <TargetFramework>net8.0</TargetFramework>\n"
        "    <ImplicitUsings>enable</ImplicitUsings>\n"
        "    <Nullable>enable</Nullable>\n"
        "  </PropertyGroup>\n"
        "</Project>\n"
    );

    fclose(project);


    for (int i = 0; i < options->source_count; i++) {

        char *cp_args[] = {
            "cp",
            options->sources[i],
            directory,
            NULL
        };

        int result = execute(cp_args);

        if (result != 0)
            return result;
    }

  
    char *args[MAX_ARGS];

    int n = 0;

    args[n++] = "dotnet";
    args[n++] = "publish";
    args[n++] = project_file;

    args[n++] = "-c";

    if (options->optimize)
        args[n++] = "Release";
    else
        args[n++] = "Debug";

    args[n++] = "--self-contained";
    args[n++] = "true";

    args[n++] = "-r";
    args[n++] = "linux-x64";

    args[n++] = "-p:PublishSingleFile=true";
    args[n++] = "-p:IncludeNativeLibrariesForSelfExtract=true";

    args[n++] = "-o";

    char publish_dir[4096];

    snprintf(
        publish_dir,
        sizeof(publish_dir),
        "%s/publish",
        directory
    );

    args[n++] = publish_dir;
    args[n] = NULL;

    printf(
        "MACC: compiling %d source file(s)\n",
        options->source_count
    );

    int result = execute(args);

    if (result != 0)
        return result;

  
    char executable[4096];

    snprintf(
        executable,
        sizeof(executable),
        "%s/MaccProgram",
        publish_dir
    );

    char *cp_args[] = {
        "cp",
        executable,
        options->output,
        NULL
    };

    result = execute(cp_args);

    if (result != 0) {
        fprintf(stderr,
                "macc: error: failed to create '%s'\n",
                options->output);
        return result;
    }

    chmod(options->output, 0755);

    printf(
        "MACC: successfully created '%s'\n",
        options->output
    );

    return 0;
}


int main(int argc, char **argv)
{
    MaccOptions options;

    if (argc == 1) {
        print_help();
        return 0;
    }

    if (parse_arguments(argc, argv, &options) != 0)
        return 1;

    if (check_sources(&options) != 0)
        return 1;

    return compile_project(&options);
}



