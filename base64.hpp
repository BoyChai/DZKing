#include <openssl/evp.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
// Base64 ���뺯��
char* base64_encode(const unsigned char* data, size_t length) {
    EVP_ENCODE_CTX* ctx;
    int out_len = 0;
    char* encoded_data = NULL;

    // ����EVP����������
    ctx = EVP_ENCODE_CTX_new();
    if (ctx == NULL) {
        fprintf(stderr, "EVP_ENCODE_CTX_new failed\n");
        return NULL;
    }

    // ��ʼ������������
    EVP_EncodeInit(ctx);

    // ��������������������С
    int encoded_len = EVP_ENCODE_LENGTH(length);

    // �����㹻���ڴ��ű���������
    encoded_data = (char*)malloc(encoded_len + 1);
    if (encoded_data == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        EVP_ENCODE_CTX_free(ctx);
        return NULL;
    }

    // ִ�б���
    EVP_EncodeUpdate(ctx, (unsigned char*)encoded_data, &out_len, data, length);

    // ���һ�θ���
    EVP_EncodeFinal(ctx, (unsigned char*)encoded_data + out_len, &out_len);

    // ����ַ�����β��
    encoded_data[encoded_len] = '\0';

    // ������Դ
    EVP_ENCODE_CTX_free(ctx);

    return encoded_data;
}
// Base64 ���뺯��
unsigned char* base64_decode(const char* encoded_data, size_t length) {
    EVP_ENCODE_CTX* ctx;
    int out_len = 0;
    unsigned char* decoded_data = NULL;

    // ����EVP����������
    ctx = EVP_ENCODE_CTX_new();
    if (ctx == NULL) {
        fprintf(stderr, "EVP_ENCODE_CTX_new failed\n");
        return NULL;
    }

    // ��ʼ������������
    EVP_DecodeInit(ctx);

    // ��������������������С
    length = EVP_DECODE_LENGTH(strlen(encoded_data));

    // �����㹻���ڴ��Ž���������
    decoded_data = (unsigned char*)malloc(length);
    if (decoded_data == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        EVP_ENCODE_CTX_free(ctx);
        return NULL;
    }

    // ִ�н���
    EVP_DecodeUpdate(ctx, decoded_data, &out_len, (unsigned char*)encoded_data, strlen(encoded_data));

    // ���һ�θ���
    EVP_DecodeFinal(ctx, decoded_data + out_len, &out_len);

    // ���½����ĳ���
    length = out_len;

    // ������Դ
    EVP_ENCODE_CTX_free(ctx);

    return decoded_data;
}