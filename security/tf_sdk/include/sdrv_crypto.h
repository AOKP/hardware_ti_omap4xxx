/**
 * Copyright(c) 2011 Trusted Logic.   All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Trusted Logic nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * File            : sdrv_crypto.h
 *
 * Last-Author     : Trusted Logic S.A.
 * Created         : August 28, 2007
 *
 */


/**
 * SDDI Crypto Driver specification header file.
 */

#ifndef __SDRV_CRYPTO_H__
#define __SDRV_CRYPTO_H__

#include "s_type.h"
#include "s_error.h"
#include "sddi.h"

#ifndef SSDI_EXPORT
#if defined(WIN32) || defined(__ARMCC_VERSION)
#ifdef SMODULE_IMPLEMENTATION
#define SSDI_EXPORT __declspec(dllexport)
#else
#define SSDI_EXPORT __declspec(dllimport)
#endif
#elif defined(__GNUC__)
#define SSDI_EXPORT __attribute__ ((visibility ("default")))
#else
#define SSDI_EXPORT
#endif
#endif   /* !SSDI_EXPORT */

#ifndef SSPI_EXPORT
#if defined(WIN32) || defined(_WIN32_WCE) || defined(__ARMCC_VERSION)
#define SSPI_EXPORT __declspec(dllexport)
#elif defined(__GNUC__)
#define SSPI_EXPORT __attribute__ ((visibility ("default")))
#else
#define SSPI_EXPORT
#endif
#endif   /* !SSPI_EXPORT */




/*------------------------------------------------------------------------------
         SSDI Types
------------------------------------------------------------------------------*/

typedef struct SDRV_CRYPTO_KEY_INFO
{
   uint32_t nKeyType;
   uint32_t nKeyAlgorithm;
   uint32_t nKeyBits;
   uint32_t nUseType;
   uint32_t nFlags;
   uint32_t nReserved1;
   uint32_t nReserved2;
} SDRV_CRYPTO_KEY_INFO;
/* Caution: the meaning of nKeyBits depends on the algorithm and the key type */

typedef struct SDRV_CRYPTO_BIGNUM
{
   uint32_t nLength;
   uint8_t *pBuffer;
} SDRV_CRYPTO_BIGNUM;

/*------------------------------------------------------------------------------
         Algorithm-dependent structures
------------------------------------------------------------------------------*/

typedef struct SDRV_CRYPTO_RSA_PUBLIC_KEY
{
   SDRV_CRYPTO_BIGNUM modulus;
   SDRV_CRYPTO_BIGNUM publicExponent;
} SDRV_CRYPTO_RSA_PUBLIC_KEY;

typedef struct SDRV_CRYPTO_RSA_PRIVATE_KEY
{
   SDRV_CRYPTO_BIGNUM modulus;
   SDRV_CRYPTO_BIGNUM publicExponent;
   SDRV_CRYPTO_BIGNUM privateExponent;
   SDRV_CRYPTO_BIGNUM primeP;
   SDRV_CRYPTO_BIGNUM primeQ;
   SDRV_CRYPTO_BIGNUM exp1;
   SDRV_CRYPTO_BIGNUM exp2;
   SDRV_CRYPTO_BIGNUM coef;
} SDRV_CRYPTO_RSA_PRIVATE_KEY;

typedef struct SDRV_CRYPTO_DH_PUBLIC_KEY
{
   SDRV_CRYPTO_BIGNUM p;
   SDRV_CRYPTO_BIGNUM g;
   SDRV_CRYPTO_BIGNUM y;
} SDRV_CRYPTO_DH_PUBLIC_KEY;

typedef struct SDRV_CRYPTO_DH_PRIVATE_KEY
{
   SDRV_CRYPTO_BIGNUM p;
   SDRV_CRYPTO_BIGNUM g;
   SDRV_CRYPTO_BIGNUM x;
} SDRV_CRYPTO_DH_PRIVATE_KEY;

typedef struct SDRV_CRYPTO_DH_PARAMETERS
{
   SDRV_CRYPTO_BIGNUM  p;
   SDRV_CRYPTO_BIGNUM  g;
} SDRV_CRYPTO_DH_PARAMETERS;

typedef struct SDRV_CRYPTO_DSA_PARAMETER
{
   SDRV_CRYPTO_BIGNUM p;
   SDRV_CRYPTO_BIGNUM q;
   SDRV_CRYPTO_BIGNUM g;
} SDRV_CRYPTO_DSA_PARAMETERS;

typedef struct SDRV_CRYPTO_DSA_PUBLIC_KEY
{
   SDRV_CRYPTO_BIGNUM p;
   SDRV_CRYPTO_BIGNUM q;
   SDRV_CRYPTO_BIGNUM g;
   SDRV_CRYPTO_BIGNUM y;
} SDRV_CRYPTO_DSA_PUBLIC_KEY;

typedef struct SDRV_CRYPTO_DSA_PRIVATE_KEY
{
   SDRV_CRYPTO_BIGNUM p;
   SDRV_CRYPTO_BIGNUM q;
   SDRV_CRYPTO_BIGNUM g;
   SDRV_CRYPTO_BIGNUM x;
} SDRV_CRYPTO_DSA_PRIVATE_KEY;

typedef SDRV_CRYPTO_BIGNUM SDRV_CRYPTO_FIELD_ELEMENT;

typedef struct SDRV_CRYPTO_EC_POINT {
   uint32_t type;
   SDRV_CRYPTO_FIELD_ELEMENT x, y;
}  SDRV_CRYPTO_EC_POINT;

typedef struct SDRV_CRYPTO_EC_PARAMETERS
{
   uint32_t fieldType;
   SDRV_CRYPTO_BIGNUM fieldParam;
   SDRV_CRYPTO_EC_POINT generator;
   SDRV_CRYPTO_FIELD_ELEMENT a;
   SDRV_CRYPTO_FIELD_ELEMENT b;
   SDRV_CRYPTO_BIGNUM order;
   uint32_t cofactor;
   uint32_t seedLen;
   uint8_t *pSeed;
}  SDRV_CRYPTO_EC_PARAMETERS;

typedef struct SDRV_CRYPTO_EC_PUBLIC_KEY
{
  SDRV_CRYPTO_EC_PARAMETERS domainParameters;
  SDRV_CRYPTO_EC_POINT publicKey;
}  SDRV_CRYPTO_EC_PUBLIC_KEY;

typedef struct SDRV_CRYPTO_EC_PRIVATE_KEY
{
  SDRV_CRYPTO_EC_PARAMETERS domainParameters;
  SDRV_CRYPTO_BIGNUM privateKey;
}  SDRV_CRYPTO_EC_PRIVATE_KEY;

typedef struct SDRV_CRYPTO_ECSVP_DH_KDF_SHA1_PARAMETERS
{
  uint32_t sharedInfoLen;
  uint8_t *pSharedInfo;
  SDRV_CRYPTO_EC_POINT publicKey;
} SDRV_CRYPTO_ECSVP_DH_KDF_SHA1_PARAMETERS;

typedef struct SDRV_CRYPTO_EXTENDED_TYPE
{
   uint32_t nType;
   void*    pValue;
   uint32_t nLen;
} SDRV_CRYPTO_EXTENDED_TYPE;


/*------------------------------------------------------------------------------
         Constants
------------------------------------------------------------------------------*/

/* Hardware Key IDs */
#define SDRV_CRYPTO_HW_KEY_MASTER_ID            1
#define SDRV_CRYPTO_HW_KEY_CEK_CUSTOMER_ID      3
#define SDRV_CRYPTO_HW_KEY_KEK_ID               4

/* operation codes */
#define SDRV_CRYPTO_OP_DIGEST                 0x00000001
#define SDRV_CRYPTO_OP_SIGN                   0x00000002
#define SDRV_CRYPTO_OP_VERIFY                 0x00000003
#define SDRV_CRYPTO_OP_ENCRYPT                0x00000004
#define SDRV_CRYPTO_OP_DECRYPT                0x00000005

/* encoding formats */
#define SDRV_CRYPTO_ENCODING_FORMAT_NONE      0x00000000

/* key parameters */
#define SDRV_CRYPTO_KEY_PARAM_MODULUS          0x00000001
#define SDRV_CRYPTO_KEY_PARAM_PUBLIC_EXPONENT  0x00000002
#define SDRV_CRYPTO_KEY_PARAM_GENERATOR        0x00000003
#define SDRV_CRYPTO_KEY_PARAM_SUBPRIME         0x00000004
#define SDRV_CRYPTO_KEY_PARAM_PUBLIC_VALUE     0x00000005
#define SDRV_CRYPTO_KEY_PARAM_VALUE            0x00000006

#define SDRV_CRYPTO_KEY_PARAM_PRIVATE_EXPONENT 0x00000007
#define SDRV_CRYPTO_KEY_PARAM_PRIME_1          0x00000008
#define SDRV_CRYPTO_KEY_PARAM_PRIME_2          0x00000009
#define SDRV_CRYPTO_KEY_PARAM_EXPONENT_1       0x0000000A
#define SDRV_CRYPTO_KEY_PARAM_EXPONENT_2       0x0000000B
#define SDRV_CRYPTO_KEY_PARAM_COEFFICIENT      0x0000000C

#define SDRV_CRYPTO_KEY_PARAM_EC_FIELD_TYPE                  0x00000010
#define SDRV_CRYPTO_KEY_PARAM_EC_GENERATOR_X_UNCOMPRESSED    0x00000011
#define SDRV_CRYPTO_KEY_PARAM_EC_GENERATOR_Y_UNCOMPRESSED    0x00000012
#define SDRV_CRYPTO_KEY_PARAM_EC_A                           0x00000013
#define SDRV_CRYPTO_KEY_PARAM_EC_B                           0x00000014
#define SDRV_CRYPTO_KEY_PARAM_EC_ORDER                       0x00000015
#define SDRV_CRYPTO_KEY_PARAM_EC_COFACTOR                    0x00000016
#define SDRV_CRYPTO_KEY_PARAM_EC_PRIME                       0x00000017
#define SDRV_CRYPTO_KEY_PARAM_EC_REDUCTION_POLYNOMIAL        0x00000018
#define SDRV_CRYPTO_KEY_PARAM_EC_SEED                        0x00000019
#define SDRV_CRYPTO_KEY_PARAM_EC_PUBLIC_VALUE_X_UNCOMPRESSED 0x0000001A
#define SDRV_CRYPTO_KEY_PARAM_EC_PUBLIC_VALUE_Y_UNCOMPRESSED 0x0000001B
#define SDRV_CRYPTO_KEY_PARAM_EC_FIELD_SIZE                  0x0000001C

/* key types */
#define SDRV_CRYPTO_KEY_TYPE_SECRET            0x00000001
#define SDRV_CRYPTO_KEY_TYPE_PUBLIC            0x00000002
#define SDRV_CRYPTO_KEY_TYPE_PRIVATE           0x00000003
#define SDRV_CRYPTO_KEY_TYPE_DATA              0x00000004

#define SDRV_CRYPTO_FIELD_P                    0x00000001
#define SDRV_CRYPTO_FIELD_2M_POLYNOMIAL        0x00000002

/* key and algorithm usage */
#define SDRV_CRYPTO_USE_SIGN                   0x00000001
#define SDRV_CRYPTO_USE_VERIFY                 0x00000002
#define SDRV_CRYPTO_USE_ENCRYPT                0x00000004
#define SDRV_CRYPTO_USE_DECRYPT                0x00000008
#define SDRV_CRYPTO_USE_DERIVE                 0x00000010
#define SDRV_CRYPTO_USE_UNWRAP                 0x00000020
#define SDRV_CRYPTO_USE_WRAP                   0x00000040
#define SDRV_CRYPTO_USE_DIGEST                 0x00000080
#define SDRV_CRYPTO_USE_SECRET_GENERATION      0x00000100
#define SDRV_CRYPTO_USE_KEYPAIR_GENERATION     0x00000200

/* key flags */
#define SDRV_CRYPTO_FLAG_WRAPPABLE             0x00000001
#define SDRV_CRYPTO_FLAG_NON_SENSITIVE         0x00000002
#define SDRV_CRYPTO_FLAG_ALLOW_NON_SENSITIVE_DERIVED_KEY   0x00000004
#define SDRV_CRYPTO_FLAG_COPY_PROHIBITED       0x00000008

/* key storage */
#define SDRV_CRYPTO_STORAGE_TEMPORARY          0x00000001
#define SDRV_CRYPTO_STORAGE_PERMANENT          0x00000002

/* algorithm constants */
#define SDRV_CRYPTO_ALG_NONE                   0x00000000
#define SDRV_CRYPTO_ALG_RSA_RAW                0x30001001
#define SDRV_CRYPTO_ALG_RSA_PKCS1_5_NONE       0x30002001
#define SDRV_CRYPTO_ALG_RSA_PKCS1_5_MD5        0x30002601
#define SDRV_CRYPTO_ALG_RSA_PKCS1_5_SHA1       0x30002101
#define SDRV_CRYPTO_ALG_RSA_PKCS1_5_SHA224     0x30002201
#define SDRV_CRYPTO_ALG_RSA_PKCS1_5_SHA256     0x30002301
#define SDRV_CRYPTO_ALG_RSA_PKCS1_5_SHA384     0x30002401
#define SDRV_CRYPTO_ALG_RSA_PKCS1_5_SHA512     0x30002501
#define SDRV_CRYPTO_ALG_DSA                    0x30000081
#define SDRV_CRYPTO_ALG_DSA_SHA1               0x30000181
#define SDRV_CRYPTO_ALG_DH_PKCS3               0x30000082
#define SDRV_CRYPTO_ALG_ECDSA_SHA1             0x40000101
#define SDRV_CRYPTO_ALG_AES_ECB                0x10000101
#define SDRV_CRYPTO_ALG_AES_CBC                0x10000201
#define SDRV_CRYPTO_ALG_AES_CTR                0x10000301
#define SDRV_CRYPTO_ALG_AES_MAC                0x60001001
#define SDRV_CRYPTO_ALG_DES_ECB                0x10000102
#define SDRV_CRYPTO_ALG_DES_CBC                0x10000202
#define SDRV_CRYPTO_ALG_DES_MAC                0x60001002
#define SDRV_CRYPTO_ALG_DES3_EDE_ECB           0x10010104
#define SDRV_CRYPTO_ALG_DES3_EDE_CBC           0x10010204
#define SDRV_CRYPTO_ALG_DES3_EDE_MAC           0x60011004
#define SDRV_CRYPTO_ALG_ARC4                   0x20000005
#define SDRV_CRYPTO_ALG_MD5                    0x50000061
#define SDRV_CRYPTO_ALG_SHA1                   0x50000012
#define SDRV_CRYPTO_ALG_SHA224                 0x50000022
#define SDRV_CRYPTO_ALG_SHA256                 0x50000032
#define SDRV_CRYPTO_ALG_SHA384                 0x50000042
#define SDRV_CRYPTO_ALG_SHA512                 0x50000052
#define SDRV_CRYPTO_ALG_HMAC_SHA1              0x60000106
#define SDRV_CRYPTO_ALG_HMAC_SHA224            0x60000206
#define SDRV_CRYPTO_ALG_HMAC_SHA256            0x60000306
#define SDRV_CRYPTO_ALG_HMAC_SHA384            0x60000406
#define SDRV_CRYPTO_ALG_HMAC_SHA512            0x60000506
#define SDRV_CRYPTO_ALG_HMAC_MD5               0x60000606
#define SDRV_CRYPTO_ALG_WRAP_SECURE_STORAGE    0x80000000
#define SDRV_CRYPTO_ALG_GEN_RSA_PKCS1          0x90000001
#define SDRV_CRYPTO_ALG_GEN_DSA                0x90000002
#define SDRV_CRYPTO_ALG_GEN_EC                 0x90000003
#define SDRV_CRYPTO_ALG_GEN_DH_PKCS3           0x90000004
#define SDRV_CRYPTO_ALG_GEN_AES                0x90000081
#define SDRV_CRYPTO_ALG_GEN_DES                0x90000082
#define SDRV_CRYPTO_ALG_GEN_DES2               0x90000083
#define SDRV_CRYPTO_ALG_GEN_DES3               0x90000084
#define SDRV_CRYPTO_ALG_GEN_ARC4               0x90000085
#define SDRV_CRYPTO_ALG_GEN_GENERIC_SECRET     0x90000080

/* vendor defined algorithms constants */
#define SDRV_CRYPTO_ALG_VENDOR_DEFINED_0       0x100000F0
#define SDRV_CRYPTO_ALG_VENDOR_DEFINED_1       0x100000F1
#define SDRV_CRYPTO_ALG_VENDOR_DEFINED_2       0x100000F2
#define SDRV_CRYPTO_ALG_VENDOR_DEFINED_3       0x100000F3
#define SDRV_CRYPTO_ALG_VENDOR_DEFINED_4       0x100000F4
#define SDRV_CRYPTO_ALG_VENDOR_DEFINED_5       0x100000F5
#define SDRV_CRYPTO_ALG_VENDOR_DEFINED_6       0x100000F6
#define SDRV_CRYPTO_ALG_VENDOR_DEFINED_7       0x100000F7
#define SDRV_CRYPTO_ALG_VENDOR_DEFINED_8       0x100000F8
#define SDRV_CRYPTO_ALG_VENDOR_DEFINED_9       0x100000F9
#define SDRV_CRYPTO_ALG_VENDOR_DEFINED_10      0x100000FA
#define SDRV_CRYPTO_ALG_VENDOR_DEFINED_11      0x100000FB
#define SDRV_CRYPTO_ALG_VENDOR_DEFINED_12      0x100000FC
#define SDRV_CRYPTO_ALG_VENDOR_DEFINED_13      0x100000FD
#define SDRV_CRYPTO_ALG_VENDOR_DEFINED_14      0x100000FE
#define SDRV_CRYPTO_ALG_VENDOR_DEFINED_15      0x100000FF

/* key algorithm mask */
#define SDRV_CRYPTO_ALG_KEY_MASK               0xF00000FF

/* standard and restricted modes */
#define SDRV_CRYPTO_STANDARD_MODE              0x00000000
#define SDRV_CRYPTO_RESTRICTED_MODE            0x00000001

/*------------------------------------------------------------------------------
         Crypto Driver Functions
------------------------------------------------------------------------------*/

S_RESULT SDRV_EXPORT SDrvCryptoOpen(
                     OUT void** ppSessionContext);

void SDRV_EXPORT SDrvCryptoClose(
                      IN void* pSessionContext);

S_RESULT SDRV_EXPORT SDrvCryptoOpenHardwareKey(
                      IN void*      pSessionContext,
                      IN uint32_t   nHWKeyID,
                      IN uint32_t   nHWFlags,
                     OUT void**     ppKeyContext,
                     OUT SDRV_CRYPTO_KEY_INFO* pKeyInfo);

void SDRV_EXPORT SDrvCryptoCloseKeyContext(
                      IN void* pSessionContext,
                      IN void* pKeyContext);

S_RESULT SDRV_EXPORT SDrvCryptoInitOperation(
                   IN    void* pSessionContext,
                     uint32_t  nOperation,
                     uint32_t  nAlgorithm,
               IN const void*  pParameter,
                     uint32_t  nParameterLen,
                   IN    void* pKeyContext,
                  OUT   void** ppOperationContext);

S_RESULT SDRV_EXPORT SDrvCryptoEncrypt(
                  IN     void* pOperationContext,
             IN const uint8_t* pData,
                     uint32_t  nDataLen,
                 OUT  uint8_t* pEncryptedData,
                         void* pReserved,
                         bool  bTerminate);

S_RESULT SDRV_EXPORT SDrvCryptoEncryptUpdate(
                      IN void* pOperationContext,
             IN const uint8_t* pPart,
                     uint32_t  nPartLen,
                  OUT uint8_t* pEncryptedPart,
                         void* pReserved,
                         bool  bTerminate);

S_RESULT SDRV_EXPORT SDrvCryptoDecrypt(
                  IN void*     pOperationContext,
            IN const uint8_t*  pEncryptedData,
                     uint32_t  nEncryptedDataLen,
                 OUT uint8_t*  pData,
                     void*     pReserved,
                  IN bool      bTerminate);

S_RESULT SDRV_EXPORT SDrvCryptoDecryptUpdate(
               IN       void*  pOperationContext,
            IN const uint8_t*  pEncryptedPart,
                    uint32_t   nEncryptedPartLen,
               OUT   uint8_t*  pPart,
                        void*  pReserved,
                        bool   bTerminate);

S_RESULT SDRV_EXPORT SDrvCryptoSign(
                     IN void*  pOperationContext,
            IN const uint8_t*  pData,
                    uint32_t   nDataLen,
                  OUT uint8_t* pSignature,
                       void*   pReserved,
                       bool    bTerminate);

S_RESULT SDRV_EXPORT SDrvCryptoSignUpdate(
              IN       void*   pOperationContext,
           IN const uint8_t*   pPart,
                   uint32_t    nPartLen);

S_RESULT SDRV_EXPORT SDrvCryptoSignLast(
                 IN     void*  pOperationContext,
                 OUT uint8_t*  pSignature,
                        void*  pReserved,
                 IN     bool   bTerminate);

S_RESULT SDRV_EXPORT SDrvCryptoVerify(
                  IN    void*  pOperationContext,
            IN const uint8_t*  pData,
                     uint32_t  nDataLen,
            IN const uint8_t*  pSignature,
                        void*  pReserved,
                         bool  bTerminate);

S_RESULT SDRV_EXPORT SDrvCryptoVerifyUpdate(
                      IN void* pOperationContext,
             IN const uint8_t* pPart,
                     uint32_t  nPartLen);

S_RESULT SDRV_EXPORT SDrvCryptoVerifyLast(
                  IN     void* pOperationContext,
             IN const uint8_t* pSignature,
                         void* pReserved,
                         bool  bTerminate);

S_RESULT SDRV_EXPORT SDrvCryptoDigest(
                      IN void* pOperationContext,
             IN const uint8_t* pData,
                     uint32_t  nDataLen,
                 OUT  uint8_t* pDigest,
                         void* pReserved,
                     bool      bTerminate);

S_RESULT SDRV_EXPORT SDrvCryptoDigestUpdate(
                      IN void* pOperationContext,
             IN const uint8_t* pPart,
                     uint32_t  nPartLen);

S_RESULT SDRV_EXPORT SDrvCryptoDigestLast(
                  IN void*     pOperationContext,
                  OUT uint8_t* pDigest,
                      void*    pReserved,
                      bool     bTerminate);

S_RESULT SDRV_EXPORT SDrvCryptoResetOperation(
                   IN void*    pOperationContext,
                   IN uint32_t nOperation,
                   IN void*    pKeyContext,
             IN const void*    pParameter,
                      uint32_t nParameterLen);

void SDRV_EXPORT SDrvCryptoTerminateOperation(
                      IN void* pOperationContext,
                         void* pReserved);

S_RESULT SDRV_EXPORT SDrvCryptoCreateKey(
      IN void*                  pSessionContext,
  IN OUT SDRV_CRYPTO_KEY_INFO*  pKeyInfo,
IN const void*                  pKey,
         uint32_t               nKeyLen,
     OUT void**                 ppKeyContext);

S_RESULT SDRV_EXPORT SDrvCryptoStoreKey(
                 IN void*      pSessionContext,
                 IN void*      pSourceKeyContext,
      IN SDRV_CRYPTO_KEY_INFO* pSourceKeyInfo);

S_RESULT SDRV_EXPORT SDrvCryptoLoadKey(
                 IN void*      pSessionContext,
      IN SDRV_CRYPTO_KEY_INFO* pDestinationKeyInfo,
                OUT void**     ppDestinationKeyContext);

S_RESULT SDRV_EXPORT SDrvCryptoCopyObject(
     IN  void*                 pSessionContext,
     IN  void*                 pSourceKeyContext,
  IN OUT SDRV_CRYPTO_KEY_INFO* pTargetKeyInfo,
     OUT void**                ppTargetKeyContext);

S_RESULT SDRV_EXPORT SDrvCryptoGenerateSecretKey(
      IN void*                 pSessionContext,
         uint32_t              nGenerationAlgorithm,
  IN OUT SDRV_CRYPTO_KEY_INFO* pKeyInfo,
IN const void*                 pParameter,
         uint32_t              nParameterLen,
         void*                 pReserved,
         uint32_t              nReservedLen,
     OUT void**                ppKeyContext);

S_RESULT SDRV_EXPORT SDrvCryptoGenerateKeyPair(
      IN void*                 pSessionContext,
         uint32_t              nGenerationAlgorithm,
  IN OUT SDRV_CRYPTO_KEY_INFO*  pPrivateKeyInfo,
  IN OUT SDRV_CRYPTO_KEY_INFO*  pPublicKeyInfo,
IN const void*                  pParameter,
         uint32_t               nParameterLen,
         void*                 pReserved1,
         uint32_t               nReserved1Len,
         void*                  pReserved2,
         uint32_t               nReserved2Len,
     OUT void**                ppPrivateKeyContext,
     OUT void**                ppPublicKeyContext);

S_RESULT SDRV_EXPORT SDrvCryptoDeriveKey(
     IN  void*                 pSessionContext,
         uint32_t              derivationAlgorithm,
IN const void*                 pParameter,
         uint32_t              nParameterLen,
     IN  void*                 pSourceKeyContext,
  IN OUT SDRV_CRYPTO_KEY_INFO* pTargetKeyInfo,
     IN  void*                 pReserved,
         uint32_t              nReservedLen,
     OUT void**                ppTargetKeyContext);

S_RESULT SDRV_EXPORT SDrvCryptoUnwrapKey(
      IN void*                 pSessionContext,
         uint32_t              nDecryptionAlgorithm,
         uint32_t              nEncodingFormat,
IN const void*                 pParameter,
         uint32_t              nParameterLen,
      IN void*                 pDecryptionKeyContext,
IN const uint8_t*              pWrappedBuffer,
         uint32_t              nWrappedBufferLength,
  IN OUT SDRV_CRYPTO_KEY_INFO* pKeyInfo,
         void*                 pReserved,
         uint32_t              nReservedLen,
     OUT void**                ppKeyContext);

S_RESULT SDRV_EXPORT SDrvCryptoWrapKey(
                IN void*       pSessionContext,
                   uint32_t    nEncodingFormat,
                   uint32_t    nEncryptionAlgorithm,
          IN const void*       pParameter,
                   uint32_t    nParameterLen,
                IN void*       pEncryptionKeyContext,
                IN void*       pSourceKeyContext,
            IN OUT uint8_t*    pWrappedBuffer,
                   void*       pReserved);

S_RESULT SDRV_EXPORT SDrvCryptoGetKeyParam(
                 IN  void*     pKeyContext,
                     uint32_t  keyParam,
                OUT  uint8_t*  pParameter,
             IN OUT  uint32_t* pnParameterLen);

S_RESULT SDRV_EXPORT SDrvCryptoAddEntropy(
                   IN void*    pSessionContext,
             IN const uint8_t* pSeed,
                      uint32_t nSeedLen);

S_RESULT SDRV_EXPORT SDrvCryptoGenerateRandom(
                   IN void*    pSessionContext,
                     uint32_t  nReserved,
                  OUT uint8_t* pResultBuffer,
                      uint32_t nResultBufferLen);

#endif /* __SDRV_CRYPTO_H__ */
