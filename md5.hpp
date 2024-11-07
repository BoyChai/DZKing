#include <iostream>
#include <openssl/evp.h>
#include <iomanip>
#include <sstream>

std::string getMd5Hash(const std::string& input) {
    unsigned char hash[EVP_MAX_MD_SIZE];  // ��ϣֵ������

    // �����ͳ�ʼ�� EVP_MD_CTX ������
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (mdctx == nullptr) {
        throw std::runtime_error("Failed to create MD_CTX");
    }

    // ��ʼ�� MD5 ��ϣ�㷨
    if (EVP_DigestInit_ex(mdctx, EVP_md5(), nullptr) != 1) {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("Failed to initialize MD5 algorithm");
    }

    // ���¹�ϣ
    if (EVP_DigestUpdate(mdctx, input.c_str(), input.length()) != 1) {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("Failed to update MD5 hash");
    }

    // ��ȡ��ϣ���
    unsigned int len;
    if (EVP_DigestFinal_ex(mdctx, hash, &len) != 1) {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("Failed to finalize MD5 hash");
    }

    EVP_MD_CTX_free(mdctx);  // �ͷ�������

    // ����ϣֵת��Ϊʮ�������ַ���
    std::stringstream hexStream;
    for (unsigned int i = 0; i < len; ++i) {
        hexStream << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }

    return hexStream.str();  // ���� 128 λ��32 �ַ���ʮ�������ַ���
}


std::string get16bitMd5(const std::string& input) {

    std::string temp = getMd5Hash(input);
    return temp.substr(0, 16);
}
std::string get24bitMd5(const std::string& input) {

    std::string temp = getMd5Hash(input);
    return temp.substr(0, 24);
}
std::string get32bitMd5(const std::string& input) {

    std::string temp = getMd5Hash(input);
    return temp;
}