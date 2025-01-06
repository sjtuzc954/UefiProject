#include <Uefi.h>
#include <PiDxe.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/MmServicesTableLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <IndustryStandard/Q35MchIch9.h>
#include <Library/SmmServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/CpuLib.h>
#include "Pte.h"
#include "Defs.h"
#include <Protocol/SmmVariable.h>
#include <Library/MemoryAllocationLib.h>

// a3a56e56-1d23-06dc-24bf-1473ff54e629
#define MY_VARIABLE_GUID { 0xa3a56e56, 0x1d23, 0x06dc, {0x24, 0xbf, 0x14, 0x73, 0xff, 0x54, 0xe6, 0x29} }


EFI_PHYSICAL_ADDRESS gRemapPage;

STATIC EFI_SMM_VARIABLE_PROTOCOL *mSmmVariable;
STATIC EFI_MM_CPU_IO_PROTOCOL *mMmCpuIo;

#define CR0_WP    BIT16
#define CR0_PG    BIT31
#define CR4_PSE   BIT4
#define CR4_PAE   BIT5
#define EFER_LMA  BIT8

#define EFI_PAGE_1GB BASE_1GB
#define EFI_PAGE_2MB BASE_2MB
#define EFI_PAGE_4KB BASE_4KB

#define IA32_AMD64_EFER 0xC0000080

VOID
EFIAPI
PhysMemCpy(
    IN VOID   *Dest,
    IN VOID   *Src,
    IN UINT32  Len
) {
    for(UINT8 *D = Dest, *S = Src; Len--; *D++ = *S++);
}

/**
 * Checks if:
 * PG CR0 bit set
 * PAE CR4 bit set, if not, checks PSE CR4 bit
 * LMA EFER bit set
 */
BOOLEAN
EFIAPI
IsPagingEnabled(
    VOID
) {
    // check if PG bit is not set
    UINT64 Cr0 = AsmReadCr0();
    if(!(Cr0 & CR0_PG)) {
        return FALSE;
    }

    // check if PAE set, otherwise, check PSE bit.
    // if PAE = 1 - 2MB large page
    // if PAE = 0 and PSE = 1 - 4MB large page
    // if both is 0 - 2MB large page
    UINT64 Cr4 = AsmReadCr4();
    if(!(Cr4 & CR4_PAE)) {
        // check if PSE set. If it's 1, halt next operations, because
        // we're not working with 4MB pages
        if(Cr4 & CR4_PSE) {
            return FALSE;
        }
    }

    // read EFER MSR to check if LMA set
    UINT64 Efer = AsmReadMsr64(IA32_AMD64_EFER);
    if(!(Efer & EFER_LMA)) {
        return FALSE;
    }

    return TRUE;
}

/**
 * Remap PhysAddress <-> VirtAddress
 */
BOOLEAN
EFIAPI
RemapAddress(
    IN  UINT64              VirtAddress,
    IN  UINT64              PhysAddress,
    IN  UINT64              Cr3,
    OUT PAGE_SIZE_ENUMRATE *PageSize
) {
    // PT base address is 4kb aligned.
    Cr3 &= 0xFFFFFFFFFFFFF000;

    DEBUG((DEBUG_INFO, "[SmartSmm] RemapAddress\n"));
    DEBUG((DEBUG_INFO, "[SmartSmm] old address %x\n", VirtAddress));

    PTE4 Pte4;
    Pte4.Value = *(UINT64 *)(sizeof(UINT64) * ((VirtAddress >> 39) & 0x1FF) + Cr3);
    if(!Pte4.Bits.Present)
        return FALSE;

    DEBUG((DEBUG_INFO, "[SmartSmm] PTE4 bits present\n"));

    UINT64 Cr0;
    PPTE3 Ppte3 = (PPTE3)((Pte4.Bits.Pfn << EFI_PAGE_SHIFT) + ((VirtAddress >> 30) & 0x1FF) * sizeof(UINT64));
    if(Ppte3->Bits.Present) {
        if(Ppte3->Bits.Size) {
            // VirtAddress is 1GB mapped
            if(PageSize)
                *PageSize = Page1G;

            Cr0 = AsmReadCr0();
            AsmWriteCr0(Cr0 & ~CR0_WP);

            Ppte3->Bits.Pfn = ((PhysAddress & ~(EFI_PAGE_1GB - 1)) >> EFI_PAGE_SHIFT);

            AsmWriteCr0(Cr0);

            CpuFlushTlb(); 
            
            return TRUE;
        }
    } else {
        DEBUG((DEBUG_INFO, "[SmartSmm] Ppte3 bits not present\n"));
        return FALSE;
    }

    PPTE2 Ppte2 = (PPTE2)((Ppte3->Bits.Pfn << EFI_PAGE_SHIFT) + ((VirtAddress >> 21) & 0x1FF) * sizeof(UINT64));
    if(Ppte2->Bits.Present) {
        if(Ppte2->Bits.Size) {
            // VirtAddress is 2MB mapped
            if(PageSize)
                *PageSize = Page2M;
            
            Cr0 = AsmReadCr0();
            AsmWriteCr0(Cr0 & ~CR0_WP);

            Ppte2->Bits.Pfn = ((PhysAddress & ~(EFI_PAGE_2MB - 1)) >> EFI_PAGE_SHIFT);

            AsmWriteCr0(Cr0);
            
            CpuFlushTlb();

            return TRUE;
        }
    } else {
        DEBUG((DEBUG_INFO, "[SmartSmm] Ppte2 bits not present\n"));
        return FALSE;
    }

    PPTE Ppte = (PPTE)((Ppte2->Bits.Pfn << EFI_PAGE_SHIFT) + ((VirtAddress >> 12) & 0x1FF) * sizeof(UINT64));
    if(Ppte->Bits.Present) {
        // VirtAddress is 4KB mapped
        if(PageSize)
            *PageSize = Page4K;

        Cr0 = AsmReadCr0();
        AsmWriteCr0(Cr0 & ~CR0_WP);

        Ppte->Bits.Pfn = ((PhysAddress & ~(EFI_PAGE_4KB - 1)) >> EFI_PAGE_SHIFT);

        AsmWriteCr0(Cr0);

        CpuFlushTlb();

        return TRUE;
    }

    DEBUG((DEBUG_INFO, "[SmartSmm] pte bits not present\n"));
    return FALSE;
}

VOID
EFIAPI
RestoreSmramMappings(
    VOID
) {
    UINT64 Cr3 = AsmReadCr3();
    // Identity Mapping
    RemapAddress(gRemapPage, gRemapPage, Cr3, NULL);
}

/**
 * Map physical address into the SMRAM
 */
UINT64
EFIAPI
MapPhysAddrIntoSmram(
    IN UINT64 PhysAddress
) {
    UINT8 PageSize = Page4K;
    UINT64 RemapedMemory = gRemapPage;
    UINT64 Cr3 = AsmReadCr3();
    DEBUG((DEBUG_INFO, "[SmartSmm] Cr3: 0x%016lx, 0x%016lx, 0x%016lx\n", Cr3, RemapedMemory, PhysAddress));
    
    if(RemapAddress(gRemapPage, PhysAddress, Cr3, &PageSize)) {        
        if(PageSize == Page1G) {
            RemapedMemory += PhysAddress & 0x3FFFFFF;
        } else if(PageSize == Page2M) {
            RemapedMemory += PhysAddress & 0x1FFFFF;
        } else {
            RemapedMemory += PhysAddress & 0xFFF;
        }
    } else {
        RemapedMemory = 0;
    }

    return RemapedMemory;
}

EFI_STATUS
EFIAPI
CmdPhysRead(
    IN VOID   *AddressToRead,
    IN VOID   *ReceivedInfo,
    IN UINT64  LengthToRead
) {
    DEBUG((DEBUG_INFO, "[SmartSmm] Reading from physical memory\r\n"));

    BOOLEAN check = IsPagingEnabled();
    if (check == TRUE) {
        DEBUG((DEBUG_INFO, "[SmartSmm] SMI enabled\n"));
    } else {
        DEBUG((DEBUG_INFO, "[SmartSmm] SMI not enabled\n"));
    }

    // validate input
    if((!AddressToRead || !LengthToRead || !ReceivedInfo) || LengthToRead > BASE_4KB) {
        DEBUG((DEBUG_INFO, "[SmartSmm] Invalid parameters has been passed to specific command\r\n"));
        return EFI_INVALID_PARAMETER;
    }

    DEBUG((DEBUG_INFO, "[SmartSmm] Input validated\r\n"));

    // map physical address to the SMRAM
    UINT64 PhysMapped = MapPhysAddrIntoSmram(AddressToRead);
    if(PhysMapped != 0) {
        // copy data to intermediate page
        DEBUG((DEBUG_INFO, "[SmartSmm Prepare to phsmemcpy\r\n"));

        PhysMemCpy(ReceivedInfo, PhysMapped, LengthToRead);

        DEBUG((DEBUG_INFO, "[SmartSmm] 0x%016lx, 0x%016lx\r\n", PhysMapped, AddressToRead));


        RestoreSmramMappings();
    } else {
        DEBUG((DEBUG_INFO, "[SmartSmm] Unable to map physical address\r\n"));
        return EFI_ABORTED;
    }

    return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
CmdPhysWrite(
    IN VOID    *AddressToWrite,
    IN VOID    *DataToWrite,
    IN UINT64   LengthToWrite 
) {
    DEBUG((DEBUG_INFO, "[SmartSmm] Writing to the physical memory\r\n"));

    // validate input
    if((!AddressToWrite || !DataToWrite || !LengthToWrite) || LengthToWrite > BASE_4KB) {
        DEBUG((DEBUG_INFO, "[SmartSmm] Invalid parameters has been passed to specific command\r\n"));
        return EFI_INVALID_PARAMETER;
    }

    // map target physical memory to the SMRAM
    UINT64 PhysMapped = MapPhysAddrIntoSmram(AddressToWrite);
    if(PhysMapped == 0) {
        DEBUG((DEBUG_INFO, "[SmartSmm] Unable to map physical memory to the SMRAM\r\n"));
        return EFI_ABORTED;
    }

    // copy data to the target address
    PhysMemCpy(PhysMapped, DataToWrite, LengthToWrite);

    RestoreSmramMappings();

    return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
SmartSmiHandler (
    IN EFI_HANDLE DispatchHandle,
    IN CONST VOID* RegisterContext,
    IN OUT VOID* CommBuffer,
    IN OUT UINTN* CommBufferSize
    )
{
    EFI_STATUS Status;
    UINT8 APM_CNT_VALUE;
    CHAR16 *MyVariableName = L"MyVariable";
    EFI_GUID VendorGuid = MY_VARIABLE_GUID;
    UINT32 Attributes;
    UINTN VariableSize = 0;
    VOID *VariableData = NULL;
    RequestParameters *Req = NULL;

    Status = mMmCpuIo->Io.Read(mMmCpuIo, MM_IO_UINT8, ICH9_APM_CNT, 1, &APM_CNT_VALUE);
    ASSERT_EFI_ERROR(Status);

    if (APM_CNT_VALUE != 0x01)
    {
        return EFI_WARN_INTERRUPT_SOURCE_QUIESCED;
    }

    DEBUG((DEBUG_INFO, "[SmartSmm] SMI 0x%02x\n", APM_CNT_VALUE));

    Status = mSmmVariable->SmmGetVariable(
        MyVariableName,
        &VendorGuid,
        &Attributes,
        &VariableSize,
        NULL
    );
    
    DEBUG((DEBUG_INFO, "size is %ld\n", VariableSize));

    if (Status == EFI_BUFFER_TOO_SMALL) {
        VariableData = AllocateZeroPool(VariableSize > 4096 ? VariableData : 4096);
        if (VariableData == NULL) {
            DEBUG((DEBUG_INFO, "AllocateZeroPool Failed\n"));
            return EFI_WARN_INTERRUPT_SOURCE_QUIESCED;
        }
        Status = mSmmVariable->SmmGetVariable(
            MyVariableName,
            &VendorGuid,
            &Attributes,
            &VariableSize,
            VariableData
        );

        if (!EFI_ERROR(Status)) {
            DEBUG((DEBUG_INFO, "Variable Data: 0x%016lx\n", VariableData));

            for (UINT64 j = 0; j < VariableSize; ++j) {
                DEBUG((DEBUG_INFO, "0x%02x ", ((UINT8 *)VariableData)[j]));
            }
            DEBUG((DEBUG_INFO, "\n"));

        } else {
            DEBUG((DEBUG_INFO, "Get Variable failed: %r\n", Status));
            return EFI_WARN_INTERRUPT_SOURCE_QUIESCED;
        }

        Req = (RequestParameters *)VariableData;

    } else {
        DEBUG((DEBUG_INFO, "Get Variable failed: %r\n", Status));
        return EFI_WARN_INTERRUPT_SOURCE_QUIESCED;
    }


    if (Req == NULL) {
        return EFI_WARN_INTERRUPT_SOURCE_QUIESCED;
    }

    if (Req->ParameterId == 0) {
        EFI_PHYSICAL_ADDRESS BufferAddress = 0UL;
        Status = gSmst->SmmAllocatePages(AllocateAnyPages, EfiRuntimeServicesData, 1, &BufferAddress);
        ASSERT_EFI_ERROR(Status);

        VOID *DataPtr = Req->ReadParameter.TargetAddress;
        UINT64 BytesToRead = Req->ReadParameter.BytesToRead;
        DEBUG((DEBUG_INFO, "into here, 0x%016lx, %ld\n", DataPtr, BytesToRead));
        Status = CmdPhysRead(DataPtr, BufferAddress, BytesToRead);

        for (UINT64 i = 0; i < BytesToRead; ++i) {
            DEBUG((DEBUG_INFO, "%d\n", ((UINT8 *)BufferAddress)[i]));
        }

        ASSERT_EFI_ERROR(Status);
    } else if (Req->ParameterId == 1) {
        VOID *DataPtr = Req->WriteParameter.TargetAddress;
        UINT64 BytesToWrite = Req->WriteParameter.BytesToWrite;
        VOID *DataToWrite = (void *)Req->WriteParameter.DataToWrite;

        for (UINT64 i = 0; i < BytesToWrite; ++i) {
            DEBUG((DEBUG_INFO, "%d ", ((UINT8 *)DataToWrite)[i]));
        }
        DEBUG((DEBUG_INFO, "\n"));

        Status = CmdPhysWrite(DataPtr, DataToWrite, BytesToWrite);
        ASSERT_EFI_ERROR(Status);
    }

    FreePool(VariableData);
    return EFI_WARN_INTERRUPT_SOURCE_QUIESCED;
}

EFI_STATUS
EFIAPI
SmartSmmInitialize (
    IN EFI_HANDLE ImageHandle,
    IN EFI_SYSTEM_TABLE* SystemTable
    )
{
    EFI_STATUS Status;
    EFI_HANDLE Handle;

    DEBUG((DEBUG_INFO, "[SmartSmm] SmartSmmInitialize called\n"));

    Status = gMmst->MmLocateProtocol(&gEfiMmCpuIoProtocolGuid, NULL, (VOID **)&mMmCpuIo);
    ASSERT_EFI_ERROR(Status);

    Status = gSmst->SmmLocateProtocol(&gEfiSmmVariableProtocolGuid, NULL, (VOID **)&mSmmVariable);
    ASSERT_EFI_ERROR(Status);
   
    Status = gMmst->MmiHandlerRegister(SmartSmiHandler, NULL, &Handle);
    ASSERT_EFI_ERROR(Status);

    // allocate page for remaping virtual and physical addresses
    Status = gSmst->SmmAllocatePages(AllocateAnyPages, EfiRuntimeServicesData, 1, &gRemapPage);
    if(EFI_ERROR(Status)) {
        DEBUG((DEBUG_INFO, "[SmartSmm] Unable to allocate remap page\r\n"));
        return Status;
    }

    gBS->SetMem(gRemapPage, EFI_PAGE_SIZE, 0);

    return Status;
}
