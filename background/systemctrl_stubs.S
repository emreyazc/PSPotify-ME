/* ARK/PRO CFW SystemCtrlForKernel imports, NIDs from ARK-5 exports. */
    .set noreorder
#include "pspimport.s"
    IMPORT_START "SystemCtrlForKernel",0x00090000
    IMPORT_FUNC "SystemCtrlForKernel",0x159AF5CC,sctrlHENFindFunction
    IMPORT_FUNC "SystemCtrlForKernel",0xF988C1DC,sctrlHENPatchSyscall
