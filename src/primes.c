#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// sieve of eratosthenes
// size: number of BYTES in bits[]
// data: packed sieve data; eight nums to a byte;
//   lsb state of first num, msb last
struct soe {
    size_t size;
    unsigned char data[];
};

bool is_prime(size_t n, struct soe **sp) {
    struct soe *s = *sp;
    // grow sieve if it does not already include n
    if (s->size <= n >> 3) {
        // determine new size and reallocate sieve
        size_t oldsize = s->size;
        size_t newsize = s->size;
        do {
            newsize <<= 1;
        } while (newsize <= n >> 3);
        s->size = newsize;
        *sp = s = realloc(s, sizeof(*s) + s->size);
        memset(&s->data[oldsize], 0xff, newsize - oldsize);
        // iterate over all bits of the new sieve
        size_t oldsizeinbits = oldsize << 3;
        size_t newsizeinbits = newsize << 3;
        for (size_t i = 0; i < newsizeinbits; ++i) {
            // if the bit was already accounted for in the old sieve,
            // check if it was cleared as not prime, and if so, skip it
            if (i < oldsizeinbits) {
                size_t byteind = i >> 3;
                size_t bitind = i & 0b111;
                if (!(s->data[byteind] & (1 << bitind))) {
                    continue;
                }
            }
            // otherwise (that is, if it was prime)
            // go through and clear every multiple bit in the new sieve
            // except the prime bit itself
            size_t j = i << 1;
            while (j < oldsizeinbits) {
                j += i;
            }
            for (; j < newsizeinbits; j += i) {
                size_t byteind = j >> 3;
                size_t bitind = j & 0b111;
                s->data[byteind] &= ~(1 << bitind);
            }
        }
    }
    // sieve now includes n, can test for it
    size_t byteind = n >> 3;
    size_t bitind = n & 0b111;
    return !!(s->data[byteind] & (1 << bitind));
}

struct soe *make_starter_soe(void) {
    struct soe *s = malloc(sizeof(*s) + 1);
    s->size = 1;
    // first 8 correct statuses as prime or not, from 0 upward
    s->data[0] = 0b10101100;
    return s;
}

struct soe *read_soe(const char *fname) {
    // warning: the correctness of this function
    // is contingent on the file data being correct
    FILE *fin = fopen(fname, "r");
    if (fin) {
        struct soe *s = calloc(1, sizeof(*s) + 256);
        s->size = 256;
        size_t n = 0;
        size_t largest = 0;
        // iterate over file characters
        for (int c; (c = fgetc(fin)) != EOF;) {
            if (isdigit(c)) {
                // use digits to build numbers
                n = 10*n + (c - '0');
            } else if (n != 0) {
                // use non-digits as triggers to store numbers
                size_t byteind = n >> 3;
                size_t bitind = n & 0b111;
                // resize sieve as necessary to accommodate numbers
                if (byteind >= s->size) {
                    size_t oldsize = s->size;
                    size_t newsize = s->size;
                    do {
                        newsize <<= 1;
                    } while (newsize <= byteind);
                    s->size = newsize;
                    s = realloc(s, sizeof(*s) + s->size);
                    memset(&s->data[oldsize], 0, newsize - oldsize);
                }
                s->data[byteind] |= 1 << bitind;
                if (byteind > largest) {
                    largest = byteind;
                }
                // reset n
                n = 0;
            }
        }
        fclose(fin);
        // finalize sieve size to how much data we actually read
        s->size = largest + 1;
        s = realloc(s, sizeof(*s) + s->size);
        // safeguard against empty files
        if (s->size == 1) {
            s->data[0] = 0b10101100;
        }
        return s;
    } else {
        return NULL;
    }
}

bool write_soe(struct soe *s, const char *fname) {
    FILE *fout = fopen(fname, "w");
    if (fout) {
        for (size_t i = 0; i < s->size; ++i) {
            for (size_t j = 0; j < 8; ++j) {
                if (s->data[i] & (1 << j)) {
                    fprintf(fout, "%zu;\n", (i << 3) + j);
                }
            }
        }
        fclose(fout);
        return true;
    } else {
        return false;
    }
}

void print_soe(struct soe *s) {
    for (size_t i = 0; i < s->size; ++i) {
        for (size_t j = 0; j < 8; ++j) {
            if (s->data[i] & (1 << j)) {
                printf("%zu\n", (i << 3) + j);
            }
        }
    }
}

void print_prime_factorization(int n, struct soe **sp) {
    struct soe *s = *sp;
    if (is_prime(n, &s)) {
        // the conditional ensures enough primes are calculated
        // even if n itself isn't prime
        printf("%d\n", n);
    } else {
        while (n > 1) {
            for (size_t i = 0; i < s->size; ++i) {
                for (size_t j = 0; j < 8; ++j) {
                    if (s->data[i] & (1 << j)) {
                        size_t m = (i << 3) + j;
                        if (n % m == 0) {
                            printf("%zu\n", m);
                            n /= m;
                            goto double_break;
                            // using a goto as a multi-break is justifiable
                            // that's my story and I'm sticking to it
                        }
                    }
                }
            } double_break: (void) 0;
        }
    }
    *sp = s;
}

void find_and_read_soe(struct soe **sp, char **fname) {
    // determine file to use and read sieve
    char *home = getenv("HOME");
    size_t home_len = strlen(home);
    char *primes_file = getenv("PRIMES_FILE");
    char home_local_var[home_len + strlen("/.local/var/list-of-primes") + 1];
    home_local_var[0] = '\0';
    strcat(home_local_var, home);
    strcat(home_local_var, "/.local/var/list-of-primes");
    char home_config[home_len + strlen("/.config/list-of-primes") + 1];
    home_config[0] = '\0';
    strcat(home_config, home);
    strcat(home_config, "/.config/list-of-primes");
    char home_direct[home_len + strlen("/.list-of-primes") + 1];
    home_direct[0] = '\0';
    strcat(home_direct, home);
    strcat(home_direct, "/.list-of-primes");
    char *check_paths[] = {
        home_local_var,
        home_config,
        home_direct,
        "/usr/local/var/list-of-primes",
        "/usr/var/list-of-primes",
        "/var/list-of-primes"
    };
    size_t npaths = sizeof(check_paths)/sizeof(check_paths[0]);
    struct soe *s = NULL;
    if (!primes_file) {
        for (size_t i = 0; i < npaths; ++i) {
            if ((s = read_soe(check_paths[i]))) {
                primes_file = check_paths[i];
                break;
            }
        }
    }
    if (!primes_file) {
        for (size_t i = 0; i < npaths; ++i) {
            FILE *foutck = fopen(check_paths[i], "w");
            if (foutck) {
                fclose(foutck);
                primes_file = check_paths[i];
                break;
            }
        }
    }
    if (!s) {
        s = make_starter_soe();
    }
    if (primes_file) {
        size_t pflen = strlen(primes_file);
        *fname = memcpy(malloc(pflen + 1), primes_file, pflen + 1);
    } else {
        *fname = NULL;
        fprintf(stderr,
            "Could not find a suitable path for the primes file.\n"
            "Try passing a path via environment variable PRIMES_FILE.\n"
        );
    }
    *sp = s;
}

void show_help_file(void) {
    char *check_paths[] = {
        "/usr/local/share/doc/primes/README",
        "/usr/share/doc/primes/README"
    };
    size_t npaths = sizeof(check_paths)/sizeof(check_paths[0]);
    bool found = false;
    for (size_t i = 0; i < npaths; ++i) {
        FILE *fin = fopen(check_paths[i], "r");
        if (fin) {
            for (int c; (c = fgetc(fin)) != EOF;) {
                fputc(c, stdout);
            }
            fclose(fin);
            found = true;
            break;
        }
    }
    if (!found) {
        fprintf(stderr,
            "Could not find the documentation.\n"
            "It should have been installed at one of the following paths:\n"
        );
        for (size_t i = 0; i < npaths; ++i) {
            fprintf(stderr, "%s\n", check_paths[i]);
        }
    }
}

void print_at_least_n_primes(int n, struct soe **sp) {
    struct soe *s = *sp;
    size_t i = 0;
    do {
        for (; i < s->size; ++i) {
            for (size_t j = 0; j < 8; ++j) {
                if (s->data[i] & (1 << j)) {
                    printf("%zu\n", (i << 3) + j);
                    if (--n <= 0) {
                        return;
                    }
                }
            }
        }
        is_prime(s->size << 4, &s);
        *sp = s;
    } while (n > 0);
}

void print_primes_up_to_n(int n, struct soe **sp) {
    struct soe *s = *sp;
    size_t i = 0;
    for (;;) {
        for (; i < s->size; ++i) {
            for (size_t j = 0; j < 8; ++j) {
                size_t x = (i << 3) + j;
                if (x > n) {
                    return;
                } else if (s->data[i] & (1 << j)) {
                    printf("%zu\n", x);
                }
            }
        }
        is_prime(s->size << 4, &s);
        *sp = s;
    }
}

int main(int argc, char *argv[]) {
    if (argc > 1 && !strcmp(argv[1], "--help")) {
        show_help_file();
        return 0;
    }
    struct soe *s;
    char *fname;
    find_and_read_soe(&s, &fname);
    bool dry = false;
    bool standard = true;
    for (int c; (c = getopt(argc, argv, "n:u:f:d")) != -1;) {
        switch (c) {
            case 'd': {
                dry = true;
            } break;
            case 'n': {
                standard = false;
                print_at_least_n_primes(atoi(optarg), &s);
                goto double_break;
            } break;
            case 'u': {
                standard = false;
                print_primes_up_to_n(atoi(optarg), &s);
                goto double_break;
            } break;
            case 'f': {
                standard = false;
                print_prime_factorization(atoi(optarg), &s);
                goto double_break;
            } break;
        }
    } double_break: (void) 0;
    if (standard) {
        print_soe(s);
    } else if (!dry) {
        if (!write_soe(s, fname)) {
            fprintf(stderr, "Could not update the primes file.\n");
        }
    }
    free(s);
    free(fname);
    return 0;
}
