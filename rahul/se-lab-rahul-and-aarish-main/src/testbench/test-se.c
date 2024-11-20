/******************x********************************************************
 * STUDENTS: DO NOT MODIFY.
 * 
 * C S 429 system emulator
 * 
 * test_se.c - Test harness for the emulator.
 * 
 * Copyright (c) 2023. 
 * Authors: Z. Leeper., K. Chandrasekhar, K. Rathod
 * All rights reserved.
 * May not be used, modified, or copied without permission.
 **************************************************************************/ 

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_STR 1024  /* Max string size */

int verbosity;

/* 
 * usage - Prints usage info
 */
void usage(char *argv[]){
    printf("Usage: %s [-hwv]\n", argv[0]);
    printf("Options:\n");
    printf("  -h        Print this help message.\n");
    printf("  -w <num>  Test a specific week. Defaults to week 2.\n");
    printf("  -v <num>  Verbosity level. Defaults to 0, which only shows final score.\n            Set to 1 to view which tests are failing.\n            Set to 2 to view all tests as they run.\n");
#ifdef EC
    printf("  -e        Test extra credit test cases. Disabled by default.\n");
#endif
}

/*
 * SIGALRM handler 
 */
void sigalrm_handler(int signum)
{
    printf("Error: Program timed out.\n");
    exit(EXIT_FAILURE);
}

/* 
 * runtrace - Runs the student and reference solution with provided parameters,
 * on a given test, and collects the results for the caller. 
 * Return 0 if any problems, 1 if OK.
 */
static int run_test(int wk, int A, int B, int C, int d, char *testdir, char *testname, int limit) {
    int status;
    char cmd[MAX_STR];
    char checkpoint_ref[MAX_STR >> 4];
    char checkpoint_student[MAX_STR >> 4];
    char testfile[MAX_STR >> 4];

    checkpoint_ref[0] = 0;
    checkpoint_student[0] = 0;
    strcpy(checkpoint_ref, "checkpoints/checkpoint_ref_");
    strcpy(checkpoint_student, "checkpoints/checkpoint_student_");
    strcat(checkpoint_ref, testname);
    strcat(checkpoint_student, testname);
    strcat(checkpoint_ref, ".out");
    strcat(checkpoint_student, ".out");

    testfile[0] = 0;
    strcpy(testfile, testdir);
    strcat(testfile, testname);

    /* Run the reference emulator */
    sprintf(cmd, "./bin/se-ref-wk%d -A %d -B %d -C %d -d %d -l %d -i %s -c %s > /dev/null 2> /dev/null", 
           wk, A, B, C, d, limit, testfile, checkpoint_ref);
    status = system(cmd);
    if (status == -1) {
        fprintf(stderr, "Error invoking system() for reference sim: %s\n", 
                strerror(errno));
        return 0;
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        fprintf(stderr, "Error running reference emulator: Status %d\n", 
                WEXITSTATUS(status));
        return 0;
    }

    /* Run the student emulator */
    if (verbosity > 1) {
         sprintf(cmd, "./bin/se -A %d -B %d -C %d -d %d -l %d -i %s -c %s > /dev/null", 
            A, B, C, d, limit, testfile, checkpoint_student);
    }
    else {
        sprintf(cmd, "./bin/se -A %d -B %d -C %d -d %d -l %d -i %s -c %s > /dev/null 2> /dev/null", 
            A, B, C, d, limit, testfile, checkpoint_student);
    }
    
    status = system(cmd);
    if (status == -1) {
        if (verbosity > 0) {
            fprintf(stderr, "Error invoking system() for reference sim: %s\n", 
                strerror(errno));
        }
        return 0;
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        if (verbosity > 0) {
            fprintf(stderr, "Error running student emulator: Status %d\n", 
                WEXITSTATUS(status));
        }
        return 0;
    }

    /* Diff the results */
    sprintf(cmd, "diff %s %s", checkpoint_ref, checkpoint_student);
    status = system(cmd);

    int pass = 1;
    // Conveniently, diff gives a nonzero exit status if they don't match.
    if (status) pass = 0;
    
    /* Cleanup */
    sprintf(cmd, "rm -f %s", checkpoint_ref);
    status = system(cmd);
    if (status == -1) {
        fprintf(stderr, "Error removing file %s: %s\n", 
                checkpoint_ref, strerror(errno));
        return 0;
    }
    sprintf(cmd, "rm -f %s", checkpoint_student);
    status = system(cmd);
    if (status == -1) {
        fprintf(stderr, "Error removing file %s: %s\n", 
                checkpoint_student, strerror(errno));
        return 0;
    }

    return pass;
}

int main(int argc, char* argv[]) {
    char c;
    int week = 2;
    verbosity = 0;
    int ec = 0;
    /* Parse command line args. */
    while ((c = getopt(argc, argv, ":h:w:v:e")) != -1) {
        switch(c) {
        case 'h':
            usage(argv);
            exit(EXIT_SUCCESS);
        case 'w':
            week = atoi(optarg);
            break;
        case 'v':
            verbosity = atoi(optarg);
            break;
#ifdef EC
        case 'e':
            ec = 1;
            break;
#endif
        default:
            usage(argv);
            exit(EXIT_FAILURE);
        }
    }

    if(week == 1) week = 2;

    /* Check for valid command line args. */
    if (!ec && (week < 2 || week > 4)) {
        fprintf(stderr, "Error: Week must be between 2 and 4 or left unspecified.\n");
        exit(EXIT_FAILURE);
    }

#ifdef EC
    if (ec && week == 2) {
        fprintf(stderr, "Error: Week must be at least 3 to test extra credit.\n");
        exit(EXIT_FAILURE);
    }
#endif

    /* Install timeout handler */
    if (signal(SIGALRM, sigalrm_handler) == SIG_ERR) {
        fprintf(stderr, "Unable to install SIGALRM handler\n");
        exit(EXIT_FAILURE);
    }

    /* Time out and give up after a while in case of infinite loops */
    alarm(200);

    double score = 0.0;

    /* Run week 2 tests. */
    if (week == 2) {
        if (verbosity > 1) {
            fprintf(stderr, "Running tests for week 2...\n");
        }
        size_t num_testdirs = 6;
        size_t total_num_tests = 0;
        size_t test_sizes[] = {5, 7, 4, 11, 4, 7};
        char *testdirs[] = {"testcases/basics/", "testcases/alu/simple/", "testcases/mem/simple/", 
                            "testcases/alu/print_simple/", "testcases/branch/simple/", 
                            "testcases/exceptions/simple/"};
        char *test_basic[] = {"basic", "add", "sub", "movz", "movk"};
        char *test_alu[] = {"adds", "cmn", "ands", "tst", "orr", "eor", "mvn"}; // "cmp", "subs"
        char *test_mem[] = {"ldur_stur", "adrp", "adrp2", "adrp3"};
        char *test_print_alu[] = {"add", "movk_lsl", "movz_lsl", "adds", "subs", "cmp", 
                                  "ands", "orr", "movk_lsl2", "read_after_write", 
                                  "write_after_read"};
        char *test_branch[] = {"branch_taken", "branch_not_taken", "bcond", "bcond_cmn"}; //bl
        char *test_exception[] = {"non_charm_insn", "ldur", "stur", "bad_insn_1", "bad_insn_2", 
                                  "bad_insn_3", "bad_insn_4"};
        char **tests[] = {test_basic, test_alu, test_mem, test_print_alu, 
                          test_branch, test_exception};
        int total = 0;
        double weights[] = {0.1, 0.25, 0.25, 0.15, 0.15, 0.1};
        double this_score = 0.0;
        
        for (size_t dirnum = 0; dirnum < num_testdirs; dirnum++) {
            int num_tests = 0;
            double passed_this_dir = 0;
            for (size_t testnum = 0; testnum < test_sizes[dirnum]; testnum++){
                if (verbosity > 1) {
                    fprintf(stderr, "Running %s%s\n", testdirs[dirnum], tests[dirnum][testnum]);
                }
                int pass = run_test(2, -1, -1, -1, -1, testdirs[dirnum], tests[dirnum][testnum], 500);
                passed_this_dir += pass;
                if (!pass && verbosity > 0) {
                    fprintf(stderr, "Failed test %s%s\n", testdirs[dirnum], tests[dirnum][testnum]);
                }
                num_tests++;
            }
            total_num_tests += num_tests;
            total += passed_this_dir;
            this_score += (weights[dirnum]*(passed_this_dir/num_tests));
        }
        // if (week > 1) {
        //     this_score /= 4;
        // }
        score += this_score;
        if (verbosity > 0) {
            fprintf(stderr, "Total week 2 tests passed: %d of %ld.\n", total, total_num_tests);
        }
    }
    /* Run week 3 tests. */
    if (week == 3) {
        if (verbosity > 1) {
            fprintf(stderr, "Running tests for week 3...\n");
        }
        size_t num_testdirs = 6;
        size_t total_num_tests = 0;
        size_t test_sizes[] = {25, 3, 6, 19, 3, 3};
        char *testdirs[] = {"testcases/alu/hazard/", "testcases/mem/hazard/", "testcases/branch/hazard/", 
                            "testcases/exceptions/hazard/", "testcases/applications/hazard/", 
                            "testcases/applications/hard/"};
        char *test_alu[] = {"add", "add_hazards", "add2", "adds", "adds_hazards", "adds2", "ands", "ands2",
                            "cmn", "cmp_hazards", "eor", "movk_hazards", "movk_lsl", "movz_hazards",
                            "movz_lsl", "mvn", "orr", "orr2", "shift", "sub", "subs", "subs_hazards", 
                            "tst", "read_after_write", "write_after_read"}; // "cmp", "subs2", "cmp2"
        char *test_mem[] = {"adrp_hazards", "ldur", "stur"};
        char *test_branch[] = {"branch_taken", "branch_not_taken", "ret_hazard", "bcond_cmn", "bl_ret", "bl"};
        char *test_exception[] = {"bad_insn_1", "bad_insn_2", 
                                  "bad_insn_3", "bad_insn_4", "bad_mem_1", "bad_mem_2", 
                                  "bad_mem_3", "bad_mem_4", "bad_mem_5", "bad_mem_6", 
                                  "bad_mem_7", "bad_mem_8", "bad_mem_9", "bad_mem_10", 
                                  "bad_mem_11", "bad_mem_12", "bad_mem_13", "bad_mem_14",
                                  "bad_mem_15"};
        char *test_apps[] = {"13factorial", "80thfib", "reverse_long"};
        char *test_hard[] = {"iter_sum", "knapsack_simple", "rec_sum"};
        char **tests[] = {test_alu, test_mem, test_branch, test_exception, test_apps, test_hard};
        int total = 0;
        double weights[] = {0.2, 0.2, 0.2, 0.1, 0.2, 0.1};
        double this_score = 0.0;
        
        for (size_t dirnum = 0; dirnum < num_testdirs; dirnum++) {
            double num_tests = 0;
            double passed_this_dir = 0;
            for (size_t testnum = 0; testnum < test_sizes[dirnum]; testnum++){
                if (verbosity > 1) {
                    fprintf(stderr, "Running %s%s\n", testdirs[dirnum], tests[dirnum][testnum]);
                }
                int pass = run_test(3, -1, -1, -1, -1, testdirs[dirnum], tests[dirnum][testnum], 500);
                passed_this_dir += pass;
                if (!pass && verbosity > 0) {
                    fprintf(stderr, "Failed test %s%s\n", testdirs[dirnum], tests[dirnum][testnum]);
                }
                num_tests++;
            }
            total_num_tests += num_tests;
            total += passed_this_dir;
            this_score += (weights[dirnum]*(passed_this_dir/num_tests));
        }
        // if (week > 2) {
        //     this_score /= 4;
        // }
        // else {
        //     this_score *= 0.75;
        // }
        score += this_score;
        if (verbosity > 0) {
            fprintf(stderr, "Total week 3 tests passed: %d of %ld.\n", total, total_num_tests);
        }
    }
    /* Run week 3 tests. */
    if (week == 4) {
        if (verbosity > 1) {
            fprintf(stderr, "Running tests for week 4...\n");
        }
        size_t num_testdirs = 2;
        size_t total_num_tests = 0;
        size_t test_sizes[] = {2, 4};
        size_t num_cache_configs[] = {4, 3};
        char *testdirs[] = {"testcases/mem/simple/", "testcases/applications/hard/"};
        char *test_simple[] = {"ldur_stur", "adrp"};
        char *test_sum[] = {"iter_sum", "rec_sum", "knapsack_simple"};
        char **tests[] = {test_simple, test_sum};
        int cache_configs[][4] = {{1, 8, 8, 2}, {4, 8, 32, 4}, {1, 32, 64, 8}, {2, 8, 64, 8},
                                  {1, 8, 8, 2}, {4, 8, 32, 4}, {1, 32, 64, 8}, {2, 8, 64, 8},
                                  {4, 32, 512, 100}};
        double weights[] = {0.2, 0.8};        
        int total = 0;
        int cachenum = 0;
        double this_score = 0.0;

        for (size_t dirnum = 0; dirnum < num_testdirs; dirnum++) {
            int num_tests = 0;
            double passed_this_dir = 0;
            for (int i = 0; i < num_cache_configs[dirnum]; i++,cachenum++) {
                for (size_t testnum = 0; testnum < test_sizes[dirnum]; testnum++){
                    if (verbosity > 1) {
                        fprintf(stderr, "Running %s%s with cache config [A,B,C,d] = [%d,%d,%d,%d]\n",
                                testdirs[dirnum], tests[dirnum][testnum], cache_configs[cachenum][0], cache_configs[cachenum][1], 
                                cache_configs[cachenum][2], cache_configs[cachenum][3]);
                    }
                    int pass = run_test(4, cache_configs[cachenum][0], cache_configs[cachenum][1], 
                                                cache_configs[cachenum][2], cache_configs[cachenum][3],
                                                testdirs[dirnum], tests[dirnum][testnum], 1 << 26);
                    passed_this_dir += pass;
                    if (!pass && verbosity > 0) {
                        fprintf(stderr, "Failed test %s%s\n", testdirs[dirnum], tests[dirnum][testnum]);
                    }
                    num_tests++;
                }
            }
            total_num_tests += num_tests;
            total += passed_this_dir;
            this_score += (weights[dirnum]*(passed_this_dir/num_tests));
        }
        // this_score /= 2;
        score += this_score;
        if (verbosity > 0) {
            fprintf(stderr, "Total week 4 tests passed: %d of %ld.\n", total, total_num_tests);
        }
    }

    // Needs to be stdout for gradescope
    if(week == 2) {
        score *= 6;
    } else if (week == 3) {
        score *= 4;
    }

    if(1 < week && week < 5) {
        printf("Total score for week %d SE: %0.2f\n", week, score);
    }

#ifdef EC
    double ec_score = 0;
        //EXTRA CREDIT
    if(ec) {
        if (verbosity > 1) {
            fprintf(stderr, "Running tests for charmv3 (extra credit)...\n");
        }

        int cache_config_ec[] = {1, 8, 8, 2};
        if(week < 4) {
            cache_config_ec[0] = -1;
            cache_config_ec[1] = -1;
            cache_config_ec[2] = -1;
            cache_config_ec[3] = -1;
        }

        char* testdir = week == 2 ? "testcases/charmv3/simple/" : "testcases/charmv3/hazard/";

        char* tests_ec[] = {"csel_simple", "csel_less_simple", "csinc_simple", 
            "csinc_less_simple", "csinv_simple", "csinv_less_simple", "csneg_simple",
            "csneg_less_simple", "cbz", "cbnz", "knapsack", "br", "blr", "csel_four"};
        
        size_t num_tests_ec = 14;
        double passed_ec = 0;
        for (size_t testnum = 0; testnum < num_tests_ec; testnum++){
            if (verbosity > 1) {
                fprintf(stderr, "Running %s%s with cache config [A,B,C,d] = [%d,%d,%d,%d]\n",
                        testdir, tests_ec[testnum], cache_config_ec[0], cache_config_ec[1], 
                        cache_config_ec[2], cache_config_ec[3]);
            }
            int pass = run_test(3, cache_config_ec[0], cache_config_ec[1], 
                                        cache_config_ec[2],cache_config_ec[3],
                                        testdir, tests_ec[testnum], 1 << 26);
            passed_ec += pass;
            if (!pass && verbosity > 0) {
                fprintf(stderr, "Failed test %s%s\n", testdir, tests_ec[testnum]);
            }
        }

        int total_ec = passed_ec;

        double weight_ec = 2.0;

        ec_score += weight_ec*(passed_ec/num_tests_ec);

        if (verbosity > 0) {
            fprintf(stderr, "Total charmv3 tests passed: %d of %ld.\n", total_ec, num_tests_ec);
        }

        printf("Total score for charmv3: %0.2f\n", ec_score);
    }
#endif
    exit(EXIT_SUCCESS);
}
