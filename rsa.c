#include "stdio.h"
#include "string.h"
#include "openssl/rsa.h"
#include "openssl/pem.h"
#include "openssl/evp.h"
#include "openssl/err.h"
#include "openssl/ssl.h"
#include "sys/syslog.h"


/* 函数名: int rsa_init()
 * 功能:
 * 参数:
 * 返回值:
 */
int rsa_init(){
    SSL_library_init();
    ERR_load_BIO_strings();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    return 1;
}

/* 函数名: int rsa_gen_keys(const int g_nbits,char *pub_key,char *priv_key)
 * 功能: 生成rsa秘钥
 * 参数:
 * 返回值:
 */
int rsa_gen_keys(const int g_nbits,char *pub_key,char *priv_key){
	/*---------------------------------------------------
	 *说明：
	 *g_nBits表示加密长度，意味着最多可以编解码g_nBits/8-11=117个字节，
	 *RSA_F4即65537，为公钥指数，一般情况下使用RSA_F4即可，其它两个参数
	 *可以设置为NULL，为了兼容Crypto++密码库生成的密钥，公钥使用传统PEM
	 *公钥格式进行存取，私钥则采用PKCS#8非加密私钥格式进行存取
	---------------------------------------------------*/	
	
	/*对RSA结构体和EVP_KEY结构体进行初始化*/
	RSA *pRsa = RSA_new();
	EVP_PKEY *eRsa = EVP_PKEY_new();

	/*生成密钥对并保存在RSA结构体中*/
	pRsa = RSA_generate_key(g_nbits,RSA_F4,NULL,NULL);
	if (pRsa == NULL)
	{
		printf("Rsa_generate_key error\n");
		return -1;
	}

	/*建立一个保存公钥的可读/写内存BIO*/
	BIO *pub = BIO_new(BIO_s_mem());

	/*从RSA结构体中提取公钥到BIO中*/
	PEM_write_bio_RSA_PUBKEY(pub,pRsa);

	/*BIO中的公钥保存到char数组中*/
	int pub_len = BIO_read(pub,pub_key,4096);
	if(pub_len == 0)
	{
		/*printf("Generate Publickey error\n");*/
		return -2;
	}
	pub_key[pub_len] = '\0';
		
	/*释放存放公钥的BIO内存*/
	BIO_free(pub);

	/*建立一个保存私钥的可读/写内存BIO*/
	BIO *pri = BIO_new(BIO_s_mem());
	
	/*用EVP_PKEY结构体替换RSA结构体*/
	EVP_PKEY_assign_RSA(eRsa,pRsa);

	/*从EVP_PKEY结构体中提取私钥到BIO中*/
	PEM_write_bio_PKCS8PrivateKey(pri,eRsa,NULL,NULL,0,NULL,NULL);
	
	/*将BIO中的公钥保存到char数组中*/
	int pri_len = BIO_read(pri,priv_key,4096);
	if(pri_len == 0)
	{
		/*printf("Generate Privatekey error\n");*/
		return -3;
	}
	priv_key[pri_len] = '\0';

	/*释放存放私钥的BIO内存和EVP_PKEY结构体*/
	BIO_free(pri);
	EVP_PKEY_free(eRsa);		/*EVP_PKEY结构体已经替换了RSA结构体，无需再释放RSA结构体*/

	return 1;
}
/*-------------------------------------------------------
 对公钥进行PEM格式化
-------------------------------------------------------*/
void PubKeyPEMFormat(char *pubkey)
{
    char format_pubkey[4096] = {0};
    char pub_tem[4096] = {0};
    char *pub_begin = "-----BEGIN PUBLIC KEY-----\n";
    char *pub_end = "-----END PUBLIC KEY-----\n";
    char *check = strstr(pubkey,pub_begin);
    if(check)
    {
        return;
    }
    else
    {
        int nPublicKeyLen = strlen(pubkey); 
        int index = 0,publength = 0;
        memcpy(format_pubkey,pub_begin,27);
        for(index = 0; index < nPublicKeyLen; index += 64){			
            memcpy(pub_tem,pubkey+index,64);
            strcat(format_pubkey,pub_tem);
            publength = strlen(format_pubkey);
            format_pubkey[publength] = '\n';
            memset(pub_tem, 0, sizeof(pub_tem));
        }
        strcat(format_pubkey,pub_end);
        memcpy(pubkey,format_pubkey,strlen(format_pubkey));
    }
}


/*-------------------------------------------------------
 对私钥进行PKCS#8非加密的PEM格式化
-------------------------------------------------------*/
void PrivKeyPEMFormat(char *privkey){
	char format_privkey[4096] = {0};
	char priv_tem[4096] = {0};
	char *priv_begin = "-----BEGIN PRIVATE KEY-----\n";
	char *priv_end = "-----END PRIVATE KEY-----\n";
	char *check = strstr(privkey, priv_begin); 
	if(check)
	{
	    return;
	}
	else
	{
	    int nPrivateKeyLen = strlen(privkey); 
	    int index = 0,privlength = 0;
	    memcpy(format_privkey,priv_begin,28);
	    for(index = 0; index < nPrivateKeyLen; index += 64)
	    {			
	        memcpy(priv_tem,privkey+index,64);
	        strcat(format_privkey,priv_tem);
	        privlength = strlen(format_privkey);
	        format_privkey[privlength] = '\n';
	        memset(priv_tem, 0, sizeof(priv_tem));
	    }
	    strcat(format_privkey,priv_end);
	    memcpy(privkey,format_privkey,strlen(format_privkey));
	}
}


/*-------------------------------------------------------
 通过公钥长度获取加密长度
-------------------------------------------------------*/
int getEncryptLengthByPubKey(int pubKeyLen){
    int cryLen = 0;								/*加密长度*/
    switch (pubKeyLen)
    {
        case 134:	/*256*/
            cryLen = 256;
            break;
        case 178:	/*512*/
            cryLen = 512;
            break;
        case 272:	/*1024*/
            cryLen = 1024;
            break;
        case 451:	/*2048*/
            cryLen = 2048;
            break;
        case 796:	/*4096*/
            cryLen = 4096;
            break;
        default:
            break;
    }
    return cryLen;
}


/*-------------------------------------------------------
 根据私钥长度判断对应的加密长度，得出实际密文分段长度
-------------------------------------------------------*/
int getCipherRealLenByPriKey(int priKeyLen){
    /*相应加密长度对应的每段密文长度*/
    int SignleRealLen = 0;
    if(priKeyLen == 319 || priKeyLen == 323)			/*256*/
        SignleRealLen = 32;								
    else if(priKeyLen == 518 || priKeyLen == 522)		/*512*/
        SignleRealLen = 64;								
    else if(priKeyLen == 912 || priKeyLen == 916)		/*1024*/
        SignleRealLen = 128;								
    else if(priKeyLen == 1700 || priKeyLen == 1704)		/*2048*/
        SignleRealLen = 256;									
    else if(priKeyLen == 3268 || priKeyLen == 3272)		/*4096*/
        SignleRealLen = 512;									
    return SignleRealLen;
}


/*-------------------------------------------------------
 Base64编解码
-------------------------------------------------------*/
/*Base64编码*/
int Base64Encode(const char *encoded, int encodedLength, char *decoded){
    return EVP_EncodeBlock((unsigned char*)decoded, (const unsigned char*)encoded, encodedLength);
}

/*Base64解码*/
int Base64Decode(const char *encoded, int encodedLength, char *decoded){      
    return EVP_DecodeBlock((unsigned char*)decoded, (const unsigned char*)encoded, encodedLength); 
}


/*-------------------------------------------------------
 利用公钥加密明文的过程
-------------------------------------------------------*/
int rsa_pub_encrypt(char *pubkey,char *in_plain,char *cipher){
	char plain[4096] = {0};			/*存放分段后的每一段明文*/
	char encrypted[4096] = {0};			/*存放每一段明文的解密结果*/
	char result[4096] = {0};			/*存放拼接后的密文*/
	char plain_rest[4096] = {0};		/*存放分段之后剩余部分的明文*/
	char encrypted_rest[4096] = {0};		/*存放对剩余部分明文的解密结果*/
	
	/*对公钥进行PEM格式化*/
	PubKeyPEMFormat(pubkey);
	
	/*根据公钥长度进行相关的计算*/
	int pubKeyLen = strlen(pubkey);							/*计算公钥长度*/
	int CryLen = getEncryptLengthByPubKey(pubKeyLen);				/*通过公钥长度获取加密长度*/
	int maxPlain = CryLen / 8 - 11;							/*通过加密长度获取明文的最大加密长度*/
	int cipherLen = CryLen / 8;								/*通过加密长度获取密文的长度*/

	/*从字符串读取RSA公钥*/
	BIO *enc = NULL; 
	if ((enc = BIO_new_mem_buf(pubkey, -1)) == NULL)        
	{     
		printf("BIO_new_mem_buf failed!\n");      
	}

	/*解析公钥*/
	RSA *rsa_pub = RSA_new();
	rsa_pub = PEM_read_bio_RSA_PUBKEY(enc, NULL, NULL, NULL);
	if(rsa_pub == NULL)
	{
		printf("Unable to read public key!\n");
		return -1; 
	}

	/******************
	 分段循环加密过程
	******************/
	int label = 0,index = 0,index_rest = 0;
	int segment = strlen(in_plain) / maxPlain;   /*分段数*/
	int rest = strlen(in_plain) % maxPlain;      /*余数*/

	/*明文长度大于最大加密长度且非整数倍*/
	if(strlen(in_plain) > maxPlain && rest != 0)
	{
		for(label = 0;label < segment; label++)
		{
			memset(plain,0,maxPlain);
			memset(encrypted,0,cipherLen);
			memcpy(plain, in_plain+index, maxPlain);		/*对明文进行分段*/
			plain[maxPlain] = '\0';
			int EncryptedLen = RSA_public_encrypt(maxPlain, plain, encrypted, rsa_pub, RSA_PKCS1_PADDING);
			if(EncryptedLen == -1 )
			{
				printf("Failed to encrypt!\n");
				return -1;
			} 
			
			/*对每一段定长密文进行拼接*/
			memcpy(result+label*cipherLen,encrypted,cipherLen);
			
			index += maxPlain;
		}
		
		/*对剩余部分明文进行加密*/
		index_rest = segment*maxPlain;
		memset(plain_rest,0,rest);
		memcpy(plain_rest, in_plain+index_rest, rest);		/*获取剩余部分明文*/
		plain_rest[rest] = '\0';
		memset(encrypted_rest,0,cipherLen);
		int EncryptedLen = RSA_public_encrypt(rest, plain_rest, encrypted_rest, rsa_pub, RSA_PKCS1_PADDING);
		if(EncryptedLen == -1 )
		{
			printf("Failed to encrypt!\n");
			return -1;
		}
		/*将剩余部分的密文拼接到整段密文中*/
		memcpy(result+label*cipherLen,encrypted_rest,cipherLen);
		
		/*对整段密文进行Base64编码*/
		Base64Encode(result, (label+1)*cipherLen, cipher);
	}

	/*明文长度等于最大加密长度的整数倍*/
	else if(strlen(in_plain) >= maxPlain && rest == 0)
	{
		for(label = 0;label < segment; label++)
		{
			memset(plain,0,maxPlain);
			memset(encrypted,0,cipherLen);
			memcpy(plain, in_plain+index, maxPlain);		/*对明文进行分段*/
			plain[maxPlain] = '\0';
			int EncryptedLen = RSA_public_encrypt(maxPlain, plain, encrypted, rsa_pub, RSA_PKCS1_PADDING);
			if(EncryptedLen == -1 )
			{
				printf("Failed to encrypt!\n");
				return -1;
			} 			
			/*拼接每段密文*/
			memcpy(result+label*cipherLen,encrypted,cipherLen);
		}
		/*对整段密文进行Base64编码*/
		Base64Encode(result, label*cipherLen, cipher);
	}

	/*明文长度小于最大加密长度*/
	else
	{
		int EncryptedLen = RSA_public_encrypt(strlen(in_plain), in_plain, encrypted, rsa_pub, RSA_PKCS1_PADDING);
		if(EncryptedLen == -1 )
		{
			printf("Failed to encrypt!\n");
			return -1;
		}
		/*对密文进行Base64编码*/
		Base64Encode(encrypted, cipherLen, cipher);
	}

	/*释放BIO内存和RSA结构体*/
	BIO_free_all(enc);
	RSA_free(rsa_pub);
	
	return 0;
}


/*-------------------------------------------------------
 利用私钥解密密文的过程
-------------------------------------------------------*/
int rsa_priv_decrypt(char *privkey,char *cipher,char *out_plain){
	char encrypted[4096] = {0};			/*存放解码后的整段密文*/
	char encrypted_result[4096] = {0};		/*存放分段后的每一段密文*/
	char decrypted[4096] = {0};			/*存放每一段密文的解密结果*/
	
	/*对私钥进行PKCS#8非加密的PEM格式化*/
	PrivKeyPEMFormat(privkey);

	/*根据私钥长度进行相关的计算*/
	int priKeyLen = strlen(privkey);							/*私钥长度*/
	int CipherRealLen = getCipherRealLenByPriKey(priKeyLen);			/*通过私钥长度获取每段密文实际长度*/
	int plainLen = CipherRealLen - 11;
    if(CipherRealLen == 0){
        syslog(LOG_DEBUG,"Error:the length of cipher is 0,stop decrypt");
        return -1;
    }

	/*从字符串读取RSA私钥*/
	BIO *dec = NULL;  
	if ((dec = BIO_new_mem_buf(privkey, -1)) == NULL)
	{     
		printf("BIO_new_mem_buf failed!\n");      
	}       
	
	/*解析私钥*/
	RSA *rsa_pri = RSA_new();
	EVP_PKEY *pri = EVP_PKEY_new();
	pri = PEM_read_bio_PrivateKey(dec, NULL, NULL, NULL);
	if(pri == NULL){
		printf("Unable to read private key!\n");
		return -1; 
	}
	
	/*将EVP_PKEY结构体转换成RSA结构体*/
	rsa_pri = EVP_PKEY_get1_RSA(pri);

	/******************
	 分段循环解密过程
	******************/
	int CipherLen = strlen(cipher);		/*Base64编码的密文长度*/
	int index = 0, label = 0, out_plainLen = 0;
	
	/*计算真实密文的段数*/
	int segment = CipherLen * 3 / 4 / CipherRealLen;
	
	memset(out_plain, 0 ,plainLen);
	
	/*对整段密文进行Base64解码*/
	Base64Decode(cipher, CipherLen, encrypted);
	
	/*将解码后的密文分段解密后合并*/
	while(label < segment)
	{
		memset(encrypted_result,0,CipherRealLen);
		memcpy(encrypted_result,encrypted+index,CipherRealLen);		/*对密文进行分段*/
		encrypted_result[CipherRealLen] = '\0';
		
		memset(decrypted, 0, plainLen);
		int DecryptedLen = RSA_private_decrypt(CipherRealLen, encrypted_result, decrypted, rsa_pri, RSA_PKCS1_PADDING);
		if(DecryptedLen == -1){
            syslog(LOG_DEBUG,"Failed to decrypt!%s\n",ERR_error_string(ERR_get_error(),NULL));
			return -1;
		}
		decrypted[DecryptedLen] = '\0';
		strcat(out_plain, decrypted);		/*将每一段的解密结果拼接到整段输出明文中*/
		out_plainLen += DecryptedLen;
		out_plain[out_plainLen] = '\0';
		index += CipherRealLen;
		label++;
	}

	/*释放BIO内存以及RSA和EVP_PKEY结构体*/
	BIO_free_all(dec);
	RSA_free(rsa_pri);
	EVP_PKEY_free(pri); 
	
	return 0;
}
