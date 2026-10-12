//---------------------------------------------------------------------------
//	CDXLPhysicalEngineConvert.h
//
//	Engine boundary: children are [project list, child plan]. A ToPG node is
//	the root of an Arrow island; ToArrow nodes are its leaves.
//---------------------------------------------------------------------------
#ifndef GPDXL_CDXLPhysicalEngineConvert_H
#define GPDXL_CDXLPhysicalEngineConvert_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLPhysical.h"

namespace gpdxl
{
enum Edxlengineconvert
{
	EdxlengineconvertIndexProjList = 0,
	EdxlengineconvertIndexChild,
	EdxlengineconvertIndexSentinel
};

class CDXLPhysicalEngineConvert : public CDXLPhysical
{
private:
	BOOL m_to_arrow;

public:
	CDXLPhysicalEngineConvert(const CDXLPhysicalEngineConvert &) = delete;

	CDXLPhysicalEngineConvert(CMemoryPool *mp, BOOL to_arrow);

	Edxlopid GetDXLOperator() const override;

	const CWStringConst *GetOpNameStr() const override;

	BOOL
	IsToArrow() const
	{
		return m_to_arrow;
	}

	void SerializeToDXL(CXMLSerializer *xml_serializer,
						const CDXLNode *node) const override;

	static CDXLPhysicalEngineConvert *
	Cast(CDXLOperator *dxl_op)
	{
		GPOS_ASSERT(nullptr != dxl_op);
		GPOS_ASSERT(EdxlopPhysicalEngineConvert == dxl_op->GetDXLOperator());

		return dynamic_cast<CDXLPhysicalEngineConvert *>(dxl_op);
	}

#ifdef GPOS_DEBUG
	void AssertValid(const CDXLNode *, BOOL validate_children) const override;
#endif
};
}  // namespace gpdxl

#endif	// !GPDXL_CDXLPhysicalEngineConvert_H
