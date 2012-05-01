#include <media/hardware/CryptoAPI.h>

#include "WVCryptoPlugin.h"

android::CryptoFactory *createCryptoFactory() {
    return new android::WVCryptoFactory;
}
