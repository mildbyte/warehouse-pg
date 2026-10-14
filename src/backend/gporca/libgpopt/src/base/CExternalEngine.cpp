//---------------------------------------------------------------------------
//	CExternalEngine.cpp
//---------------------------------------------------------------------------

#include "gpopt/base/CExternalEngine.h"

#include "gpos/common/clibwrapper.h"

#include "gpopt/base/COptCtxt.h"
#include "gpopt/base/IExternalEngine.h"
#include "gpopt/optimizer/COptimizerConfig.h"

using namespace gpopt;

const IExternalEngine *
CExternalEngineUtils::Pengine()
{
	COptCtxt *poctxt = COptCtxt::PoctxtFromTLS();
	return nullptr != poctxt ? poctxt->GetOptimizerConfig()->GetExternalEngine()
							 : nullptr;
}

namespace
{
const CHAR *custom_scan_name = nullptr;
PfnExternalEngineCreate create = nullptr;
}  // namespace

BOOL
CExternalEngineUtils::Register(const CHAR *name, PfnExternalEngineCreate pfn,
							   const CHAR *build)
{
	if (0 != clib::Strcmp(SzExternalEngineBuild(), build))
	{
		return false;
	}
	custom_scan_name = name;
	create = pfn;
	return true;
}

const CHAR *
CExternalEngineUtils::SzCustomScanName()
{
	return custom_scan_name;
}

IExternalEngine *
CExternalEngineUtils::PengineCreate(CMemoryPool *mp)
{
	return nullptr != create ? create(mp) : nullptr;
}

void
CExternalEngineUtils::AddAlternatives(CMemoryPool *mp, const CXform *pxform,
									  CExpression *pexpr, CXformResult *pxfres,
									  ULONG ulFirst)
{
	const IExternalEngine *engine = Pengine();
	if (nullptr != engine)
	{
		engine->AddAlternatives(mp, pxform, pexpr, pxfres, ulFirst);
	}
}

BOOL
CExternalEngineUtils::FColsSupported(CColRefSet *pcrs)
{
	const IExternalEngine *engine = Pengine();
	return nullptr != engine && engine->FColsSupported(pcrs);
}

BOOL
CExternalEngineUtils::FOrderSupported(COrderSpec *pos)
{
	const IExternalEngine *engine = Pengine();
	return nullptr != engine && engine->FOrderSupported(pos);
}

CExpression *
CExternalEngineUtils::PexprEngineMotion(CMemoryPool *mp,
										CExpressionHandle &exprhdl,
										CExpression *pexprMotion)
{
	const IExternalEngine *engine = Pengine();
	return nullptr != engine
			   ? engine->PexprEngineMotion(mp, exprhdl, pexprMotion)
			   : nullptr;
}
