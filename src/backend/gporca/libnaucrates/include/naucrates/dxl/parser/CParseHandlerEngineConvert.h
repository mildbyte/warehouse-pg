//---------------------------------------------------------------------------
//	CParseHandlerEngineConvert.h
//
//	Parse handler for an engine conversion (see CDXLPhysicalEngineConvert)
//---------------------------------------------------------------------------
#ifndef GPDXL_CParseHandlerEngineConvert_H
#define GPDXL_CParseHandlerEngineConvert_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLPhysicalEngineConvert.h"
#include "naucrates/dxl/parser/CParseHandlerPhysicalOp.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

class CParseHandlerEngineConvert : public CParseHandlerPhysicalOp
{
private:
	void StartElement(const XMLCh *const element_uri,
					  const XMLCh *const element_local_name,
					  const XMLCh *const element_qname,
					  const Attributes &attr) override;

	void EndElement(const XMLCh *const element_uri,
					const XMLCh *const element_local_name,
					const XMLCh *const element_qname) override;

public:
	CParseHandlerEngineConvert(const CParseHandlerEngineConvert &) = delete;

	CParseHandlerEngineConvert(CMemoryPool *mp,
							   CParseHandlerManager *parse_handler_mgr,
							   CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerEngineConvert_H
