#ifndef _endian_h_
#define _endian_h_

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    #define htole16(bytes)  (bytes)
    #define le16toh(bytes)  (bytes)
    #define htole32(bytes)  (bytes)
    #define le32toh(bytes)  (bytes)
    #define htobe16         __builtin_bswap16
    #define be16toh         __builtin_bswap16
    #define htobe32         __builtin_bswap32
    #define be32toh         __builtin_bswap32
#else
    #define htobe16(bytes)  (bytes)
    #define be16toh(bytes)  (bytes)
    #define htobe32(bytes)  (bytes)
    #define be32toh(bytes)  (bytes)
    #define htole16         __builtin_bswap16
    #define le16toh         __builtin_bswap16
    #define htole32         __builtin_bswap32
    #define le32toh         __builtin_bswap32
#endif

#endif
