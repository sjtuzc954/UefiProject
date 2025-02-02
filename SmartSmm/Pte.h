#pragma once

#pragma pack(1)

typedef union _PTE0 {
    UINT64 Value;
    struct {
        UINT64 Present : 1;
        UINT64 Writable : 1;
        UINT64 UserAccess : 1;
        UINT64 WriteThrough : 1;
        UINT64 CacheDisabled : 1;
        UINT64 Accessed : 1;
        UINT64 Ignored3 : 1;
        UINT64 Size : 1;
        UINT64 Ignored2 : 4;
        UINT64 Pfn : 36;
        UINT64 Reserved1 : 4;
        UINT64 Ignored1 : 11;
        UINT64 ExecutionDisabled : 1;
    } Bits;
} PTE0, *PPTE0;

typedef union _PTE1 {
    UINT64 Value;
    struct {
        UINT64 Present : 1;
        UINT64 Writable : 1;
        UINT64 UserAccess : 1;
        UINT64 WriteThrough : 1;
        UINT64 CacheDisabled : 1;
        UINT64 Accessed : 1;
        UINT64 Ignored3 : 1;
        UINT64 Size : 1;
        UINT64 Ignored2 : 4;
        UINT64 Pfn : 36;
        UINT64 Reserved1 : 4;
        UINT64 Ignored1 : 11;
        UINT64 ExecutionDisabled : 1;
    } Bits;
} PTE1, *PPTE1;

typedef union _PTE2 {
    UINT64 Value;
    struct {
        UINT64 Present : 1;
        UINT64 Writable : 1;
        UINT64 UserAccess : 1;
        UINT64 WriteThrough : 1;
        UINT64 CacheDisabled : 1;
        UINT64 Accessed : 1;
        UINT64 Ignored3 : 1;
        UINT64 Size : 1;
        UINT64 Ignored2 : 4;
        UINT64 Pfn : 36;
        UINT64 Reserved1 : 4;
        UINT64 Ignored1 : 11;
        UINT64 ExecutionDisabled : 1;
    } Bits;
} PTE2, *PPTE2;

typedef union _PTE3 {
    UINT64 Value;
    struct {
        UINT64 Present : 1;
        UINT64 Writable : 1;
        UINT64 User_access : 1;
        UINT64 WriteThrough : 1;
        UINT64 CacheDisabled : 1;
        UINT64 Accessed : 1;
        UINT64 Dirty : 1;
        UINT64 AccessType : 1;
        UINT64 Global : 1;
        UINT64 Ignored2 : 3;
        UINT64 Pfn : 36;
        UINT64 Reserved1 : 4;
        UINT64 Ignored3 : 7;
        UINT64 ProtectionKey : 4;
        UINT64 ExecutionDisabled : 1;
    } Bits;
} PTE3, *PPTE3;

#pragma pack()
