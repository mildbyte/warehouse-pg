//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CXformGbAgg2HashAgg.cpp
//
//	@doc:
//		Implementation of transform
//---------------------------------------------------------------------------

#include "gpopt/xforms/CXformGbAgg2HashAgg.h"

#include "gpos/base.h"

#include "gpopt/base/CExecEngine.h"
#include "gpopt/base/COptCtxt.h"
#include "gpopt/operators/CLogicalGbAgg.h"
#include "gpopt/operators/CPatternLeaf.h"
#include "gpopt/operators/CPhysicalHashAgg.h"
#include "gpopt/xforms/CXformUtils.h"
#include "naucrates/md/IMDAggregate.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CXformGbAgg2HashAgg::CXformGbAgg2HashAgg
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformGbAgg2HashAgg::CXformGbAgg2HashAgg(CMemoryPool *mp)
	: CXformImplementation(
		  // pattern
		  GPOS_NEW(mp) CExpression(
			  mp, GPOS_NEW(mp) CLogicalGbAgg(mp),
			  GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CPatternLeaf(mp)),
			  // we need to extract deep tree in the project list to check
			  // for existence of distinct agg functions
			  GPOS_NEW(mp) CExpression(mp, GPOS_NEW(mp) CPatternTree(mp))))
{
}

//---------------------------------------------------------------------------
//	@function:
//		CXformGbAgg2HashAgg::CXformGbAgg2HashAgg
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformGbAgg2HashAgg::CXformGbAgg2HashAgg(CExpression *pexprPattern)
	: CXformImplementation(pexprPattern)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CXformGbAgg2HashAgg::Exfp
//
//	@doc:
//		Compute xform promise for a given expression handle;
//		grouping columns must be non-empty
//
//---------------------------------------------------------------------------
CXform::EXformPromise
CXformGbAgg2HashAgg::Exfp(CExpressionHandle &exprhdl) const
{
	CLogicalGbAgg *popAgg = CLogicalGbAgg::PopConvert(exprhdl.Pop());
	CColRefArray *colref_array = popAgg->Pdrgpcr();
	if (0 == colref_array->Size() || exprhdl.DeriveHasSubquery(1) ||
		!CUtils::FComparisonPossible(colref_array, IMDType::EcmptEq) ||
		!CUtils::IsHashable(colref_array))
	{
		// no grouping columns, no equality or hash operators  are available for grouping columns, or
		// agg functions use subquery arguments
		return CXform::ExfpNone;
	}

	return CXform::ExfpHigh;
}

// The engine's hash aggregate also evaluates the aggregates FApplicable
// rules out for PG, such as DISTINCT ones.
static void
AddEngineHashAgg(CMemoryPool *mp, CXformResult *pxfres, CExpression *pexpr)
{
	CLogicalGbAgg *popAgg = CLogicalGbAgg::PopConvert(pexpr->Pop());
	if (COperator::EgbaggtypeIntermediate == popAgg->Egbaggtype() ||
		!CExternalEngineUtils::FEnabled(EeopHashAgg) ||
		!CExternalEngineUtils::FLargeEnough(pexpr) ||
		!CExternalEngineUtils::FColsSupported(pexpr->DeriveOutputColumns()) ||
		!CExternalEngineUtils::FScalarSupported((*pexpr)[1]))
	{
		return;
	}
	CColRefArray *pdrgpcrArgDQA = popAgg->PdrgpcrArgDQA();
	if (nullptr != pdrgpcrArgDQA && 0 == pdrgpcrArgDQA->Size())
	{
		pdrgpcrArgDQA = nullptr;
	}
	if (nullptr != pdrgpcrArgDQA)
	{
		pdrgpcrArgDQA->AddRef();
	}
	popAgg->Pdrgpcr()->AddRef();
	CExternalEngineUtils::AddAlternative(
		mp, pxfres,
		GPOS_NEW(mp) CPhysicalHashAgg(
			mp, popAgg->Pdrgpcr(), popAgg->PdrgpcrMinimal(),
			popAgg->Egbaggtype(), popAgg->FGeneratesDuplicates(), pdrgpcrArgDQA,
			CXformUtils::FMultiStageAgg(pexpr),
			CXformUtils::FAggGenBySplitDQAXform(pexpr), popAgg->AggStage(),
			!CXformUtils::FLocalAggCreatedByEagerAggXform(pexpr)),
		pexpr);
}

//---------------------------------------------------------------------------
//	@function:
//		CXformGbAgg2HashAgg::Transform
//
//	@doc:
//		Actual transformation
//
//---------------------------------------------------------------------------
void
CXformGbAgg2HashAgg::Transform(CXformContext *pxfctxt, CXformResult *pxfres,
							   CExpression *pexpr) const
{
	GPOS_ASSERT(nullptr != pxfctxt);
	GPOS_ASSERT(FPromising(pxfctxt->Pmp(), this, pexpr));
	GPOS_ASSERT(FCheckPattern(pexpr));

	// hash agg is not used with distinct agg functions
	// hash agg is not used if agg function does not have prelim func
	// hash agg is not used for ordered aggregate
	// evaluating these conditions needs a deep tree in the project list
	if (!FApplicable(pexpr))
	{
		AddEngineHashAgg(pxfctxt->Pmp(), pxfres, pexpr);
		return;
	}

	CMemoryPool *mp = pxfctxt->Pmp();
	CLogicalGbAgg *popAgg = CLogicalGbAgg::PopConvert(pexpr->Pop());
	CColRefArray *colref_array = popAgg->Pdrgpcr();
	colref_array->AddRef();

	// extract components
	CExpression *pexprRel = (*pexpr)[0];
	CExpression *pexprScalar = (*pexpr)[1];

	// addref children
	pexprRel->AddRef();
	pexprScalar->AddRef();

	CColRefArray *pdrgpcrArgDQA = popAgg->PdrgpcrArgDQA();
	if (pdrgpcrArgDQA != nullptr && 0 != pdrgpcrArgDQA->Size())
	{
		GPOS_ASSERT(nullptr != pdrgpcrArgDQA);
		pdrgpcrArgDQA->AddRef();
	}

	// create alternative expression
	CExpression *pexprAlt = GPOS_NEW(mp) CExpression(
		mp,
		GPOS_NEW(mp) CPhysicalHashAgg(
			mp, colref_array, popAgg->PdrgpcrMinimal(), popAgg->Egbaggtype(),
			popAgg->FGeneratesDuplicates(), pdrgpcrArgDQA,
			CXformUtils::FMultiStageAgg(pexpr),
			CXformUtils::FAggGenBySplitDQAXform(pexpr), popAgg->AggStage(),
			!CXformUtils::FLocalAggCreatedByEagerAggXform(pexpr)),
		pexprRel, pexprScalar);

	// add alternative to transformation result
	pxfres->Add(pexprAlt);

	AddEngineHashAgg(mp, pxfres, pexpr);
}

//---------------------------------------------------------------------------
//	@function:
//		CXformGbAgg2HashAgg::FApplicable
//
//	@doc:
//		Check if the transformation is applicable
//
//---------------------------------------------------------------------------
BOOL
CXformGbAgg2HashAgg::FApplicable(CExpression *pexpr)
{
	CExpression *pexprPrjList = (*pexpr)[1];
	ULONG arity = pexprPrjList->Arity();
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();

	for (ULONG ul = 0; ul < arity; ul++)
	{
		CExpression *pexprPrjEl = (*pexprPrjList)[ul];
		CExpression *pexprAggFunc = (*pexprPrjEl)[0];
		CScalarAggFunc *popScAggFunc =
			CScalarAggFunc::PopConvert(pexprAggFunc->Pop());

		if (popScAggFunc->IsDistinct() ||
			!md_accessor->RetrieveAgg(popScAggFunc->MDId())->IsHashAggCapable())
		{
			return false;
		}
	}

	return true;
}

// EOF
