//---------------------------------------------------------------------------
//	CParseHandlerEngineConvert.cpp
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerEngineConvert.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerProjList.h"
#include "naucrates/dxl/parser/CParseHandlerProperties.h"
#include "naucrates/dxl/parser/CParseHandlerUtils.h"

using namespace gpdxl;

XERCES_CPP_NAMESPACE_USE

CParseHandlerEngineConvert::CParseHandlerEngineConvert(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerPhysicalOp(mp, parse_handler_mgr, parse_handler_root)
{
}

void
CParseHandlerEngineConvert::StartElement(const XMLCh *const,  // element_uri
										 const XMLCh *const element_local_name,
										 const XMLCh *const,  // element_qname
										 const Attributes &attrs)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenPhysicalEngineConvert),
				 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	CDXLMemoryManager *mm = m_parse_handler_mgr->GetDXLMemoryManager();
	BOOL to_engine = CDXLOperatorFactory::ExtractConvertAttrValueToBool(
		mm, attrs, EdxltokenEngineConvertToEngine,
		EdxltokenPhysicalEngineConvert);
	BOOL ipc = CDXLOperatorFactory::ExtractConvertAttrValueToBool(
		mm, attrs, EdxltokenEngineConvertIpc, EdxltokenPhysicalEngineConvert,
		true /*is_optional*/, false /*default_value*/);
	m_dxl_node = GPOS_NEW(m_mp) CDXLNode(
		m_mp, GPOS_NEW(m_mp) CDXLPhysicalEngineConvert(m_mp, to_engine, ipc));

	// handlers are activated in the reverse of the elements' order
	CParseHandlerBase *child_parse_handler =
		CParseHandlerFactory::GetParseHandler(
			m_mp, CDXLTokens::XmlstrToken(EdxltokenPhysical),
			m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(child_parse_handler);

	CParseHandlerBase *proj_list_parse_handler =
		CParseHandlerFactory::GetParseHandler(
			m_mp, CDXLTokens::XmlstrToken(EdxltokenScalarProjList),
			m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(proj_list_parse_handler);

	CParseHandlerBase *prop_parse_handler =
		CParseHandlerFactory::GetParseHandler(
			m_mp, CDXLTokens::XmlstrToken(EdxltokenProperties),
			m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(prop_parse_handler);

	this->Append(prop_parse_handler);
	this->Append(proj_list_parse_handler);
	this->Append(child_parse_handler);
}

void
CParseHandlerEngineConvert::EndElement(const XMLCh *const,	// element_uri
									   const XMLCh *const element_local_name,
									   const XMLCh *const  // element_qname
)
{
	if (0 != XMLString::compareString(
				 CDXLTokens::XmlstrToken(EdxltokenPhysicalEngineConvert),
				 element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	GPOS_ASSERT(3 == this->Length());

	CParseHandlerProperties *prop_parse_handler =
		dynamic_cast<CParseHandlerProperties *>((*this)[0]);
	CParseHandlerUtils::SetProperties(m_dxl_node, prop_parse_handler);
	AddChildFromParseHandler(dynamic_cast<CParseHandlerProjList *>((*this)[1]));
	AddChildFromParseHandler(
		dynamic_cast<CParseHandlerPhysicalOp *>((*this)[2]));

#ifdef GPOS_DEBUG
	m_dxl_node->GetOperator()->AssertValid(m_dxl_node,
										   false /* validate_children */);
#endif

	m_parse_handler_mgr->DeactivateHandler();
}
