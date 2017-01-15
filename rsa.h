#define RSA_KEY_LENGTH 1024

int rsa_priv_decrypt(char *privkey,char *cipher,char *out_plain);
int rsa_pub_encrypt(char *pubkey,char *in_plain,char *cipher);
int rsa_gen_keys(const int g_nbits,char *pub_key,char *priv_key);
int rsa_init();
