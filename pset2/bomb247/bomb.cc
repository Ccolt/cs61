/*****************************************************************************/

//         ⊂_-
//       　  ＼＼
//        　  ＼( ͡° ͜ʖ ͡°)          welcome
//       　　    >　   ⌒ヽ
//       　     / 　 へ＼ \.
//       　　  /　　/   ＼＼                            to
//            ﾚ　ノ　　  ヽ_つ
//       　　 /　/
//       　  /　/|
//       　 (　(ヽ
//       　 |　|、＼         this bomb
//        　| 丿 ＼ ⌒)
//       　 | |　 ) /
//        ノ )　　Lﾉ
//        (_／

/*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "support.hh"
#include "phases.hh"

FILE* infile = stdin;

int main(int argc, char *argv[]) {
    // XXX TODO C++17, C++20, or Perl 7? get feedback from Paul Graham

    // When run with no arguments, the bomb reads its input lines
    // from standard input.
    // When run with one argument <file>, the bomb reads from <file>
    // until EOF, and then switches to standard input. Thus, as you
    // defuse each phase, you can add its defusing string to <file> and
    // avoid having to retype it.
    if (argc == 2 && strcmp(argv[1], "-") != 0) {
        infile = fopen(argv[1], "r");
        if (!infile) {
            printf("%s: Error: Couldn't open %s\n", argv[0], argv[1]);
            exit(8);
        }
    } else if (argc > 2) {
        printf("Usage: %s [FILE]\n", argv[0]);
        exit(8);
    }

    // XXX consider using a global constructor for this
    initialize_bomb();

    printf("THE WISE MAN: Welcome to the citadel of eternal wisdom.\n");
    printf("so dont blow up (i go back up to heaven)\n");

    char* input = read_line();       // Get input
    phase_1(input);                  // Run the phase
    phase_defused();
    printf("PHASE 1 DEFUSED.\n");

    // loops are for luzers
    input = read_line();
    phase_2(input);
    phase_defused();
    printf("PHASE 2 DEFUSED.\n");

    input = read_line();
    phase_3(input);
    phase_defused();
    printf("PHASE 3 DEFUSED.\n");

    input = read_line();
    phase_4(input);
    phase_defused();
    printf("PHASE 4 DEFUSED.\n");

    input = read_line();
    phase_5(input);
    phase_defused();
    printf("PHASE 5 DEFUSED.\n");

    input = read_line();
    phase_6(input);
    phase_defused();
    printf("PHASE 6 DEFUSED.\n");

    // Wow, they got it! But isn't something... missing? Perhaps
    // something they overlooked? Mua ha ha ha ha!
    return 0;
}
