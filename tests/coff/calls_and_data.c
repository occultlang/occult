/* The Windows side. Linked against calls_and_data.occ, which has no main. */
long run(void);
int main(void) { return run() == 115 ? 0 : 1; }
