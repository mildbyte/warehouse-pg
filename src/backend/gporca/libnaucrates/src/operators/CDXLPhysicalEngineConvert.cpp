//---------------------------------------------------------------------------
//	CDXLPhysicalEngineConvert.cpp
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLPhysicalEngineConvert.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

CDXLPhysicalEngineConvert::CDXLPhysicalEngineConvert(CMemoryPool *mp,
													 BOOL to_arrow, BOOL ipc)
	: CDXLPhysical(mp), m_to_arrow(to_arrow), m_ipc(ipc)
{
}

Edxlopid
CDXLPhysicalEngineConvert::GetDXLOperator() const
{
	return EdxlopPhysicalEngineConvert;
}

const CWStringConst *
CDXLPhysicalEngineConvert::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenPhysicalEngineConvert);
}

void
CDXLPhysicalEngineConvert::SerializeToDXL(CXMLSerializer *xml_serializer,
										  const CDXLNode *node) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenEngineConvertToArrow), m_to_arrow);
	if (m_ipc)
	{
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenEngineConvertIpc), m_ipc);
	}
	node->SerializePropertiesToDXL(xml_serializer);
	node->SerializeChildrenToDXL(xml_serializer);
	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
}

#ifdef GPOS_DEBUG
void
CDXLPhysicalEngineConvert::AssertValid(const CDXLNode *node,
									   BOOL validate_children) const
{
	GPOS_ASSERT(EdxlengineconvertIndexSentinel == node->Arity());
	CDXLNode *child = (*node)[EdxlengineconvertIndexChild];
	GPOS_ASSERT(EdxloptypePhysical ==
				child->GetOperator()->GetDXLOperatorType());
	if (validate_children)
	{
		child->GetOperator()->AssertValid(child, validate_children);
	}
}
#endif
