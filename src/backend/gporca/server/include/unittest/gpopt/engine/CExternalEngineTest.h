//---------------------------------------------------------------------------
//	CExternalEngineTest.h
//
//	Optimizes minidumps with a test external engine (see IExternalEngine)
//	and checks the plans, engine regions included.
//---------------------------------------------------------------------------
#ifndef GPOPT_CExternalEngineTest_H
#define GPOPT_CExternalEngineTest_H

#include "gpos/base.h"

namespace gpopt
{
using namespace gpos;

class CExternalEngineTest
{
public:
	static GPOS_RESULT EresUnittest();

	static GPOS_RESULT EresUnittest_RunMinidumps();
};
}  // namespace gpopt

#endif	// !GPOPT_CExternalEngineTest_H
