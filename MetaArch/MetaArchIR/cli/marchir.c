/* C entry point; all exceptions are contained by the C++ implementation. */
extern int marchir_main(int argc, char **argv);
int main(int argc, char **argv) { return marchir_main(argc, argv); }
