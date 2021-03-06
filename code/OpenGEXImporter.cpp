/*
Open Asset Import Library (assimp)
----------------------------------------------------------------------

Copyright (c) 2006-2014, assimp team
All rights reserved.

Redistribution and use of this software in source and binary forms,
with or without modification, are permitted provided that the
following conditions are met:

* Redistributions of source code must retain the above
copyright notice, this list of conditions and the
following disclaimer.

* Redistributions in binary form must reproduce the above
copyright notice, this list of conditions and the
following disclaimer in the documentation and/or other
materials provided with the distribution.

* Neither the name of the assimp team, nor the names of its
contributors may be used to endorse or promote products
derived from this software without specific prior
written permission of the assimp team.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

----------------------------------------------------------------------
*/
#ifndef ASSIMP_BUILD_NO_OPENGEX_IMPORTER

#include "AssimpPCH.h"
#include "OpenGEXImporter.h"
#include "DefaultIOSystem.h"

#include <openddlparser/OpenDDLParser.h>

#include <vector>

static const aiImporterDesc desc = {
    "Open Game Engine Exchange",
    "",
    "",
    "",
    aiImporterFlags_SupportTextFlavour,
    0,
    0,
    0,
    0,
    "ogex"
};

namespace Grammar {
    static const char *MetricType          = "Metric";
    static const char *Metric_DistanceType = "distance";
    static const char *Metric_AngleType    = "angle";
    static const char *Metric_TimeType     = "time";
    static const char *Metric_UpType       = "up";
    static const char *NameType            = "Name";
    static const char *ObjectRefType       = "ObjectRef";
    static const char *MaterialRefType     = "MaterialRef";
    static const char *MetricKeyType       = "key";
    static const char *GeometryNodeType    = "GeometryNode";
    static const char *GeometryObjectType  = "GeometryObject";
    static const char *TransformType       = "Transform";
    static const char *MeshType            = "Mesh";
    static const char *VertexArrayType     = "VertexArray";
    static const char *IndexArrayType      = "IndexArray";
    static const char *MaterialType        = "Material";
    static const char *ColorType           = "Color";
    static const char *TextureType         = "Texture";

    enum TokenType {
        NoneType = -1,
        MetricToken,
        NameToken,
        ObjectRefToken,
        MaterialRefToken,
        MetricKeyToken,
        GeometryNodeToken,
        GeometryObjectToken,
        TransformToken,
        MeshToken,
        VertexArrayToken,
        IndexArrayToken,
        MaterialToken,
        ColorToken,
        TextureToken
    };

    static const char *ValidMetricToken[ 4 ] = {
        Metric_DistanceType,
        Metric_AngleType,
        Metric_TimeType,
        Metric_UpType
    };

    static int isValidMetricType( const char *token ) {
        if( NULL == token ) {
            return false;
        }

        int idx( -1 );
        for( size_t i = 0; i < 4; i++ ) {
            if( 0 == strncmp( ValidMetricToken[ i ], token, strlen( token ) ) ) {
                idx = (int) i;
                break;
            }
        }

        return idx;
    }

    static TokenType matchTokenType( const char *tokenType ) {
        if( 0 == strncmp( MetricType, tokenType, strlen( tokenType ) ) ) {
            return MetricToken;
        } else if( 0 == strncmp( NameType, tokenType, strlen( tokenType ) ) ) {
            return NameToken;
        } else if( 0 == strncmp( ObjectRefType, tokenType, strlen( tokenType ) ) ) {
            return ObjectRefToken;
        } else if( 0 == strncmp( MaterialRefType, tokenType, strlen( tokenType ) ) ) {
            return MaterialRefToken; 
        } else if( 0 == strncmp( MetricKeyType, tokenType, strlen( tokenType ) ) ) {
            return MetricKeyToken;
        } else if( 0 == strncmp( GeometryNodeType, tokenType, strlen( tokenType ) ) ) {
            return GeometryNodeToken;
        } else if( 0 == strncmp( GeometryObjectType, tokenType, strlen( tokenType ) ) ) {
            return GeometryObjectToken;
        } else if( 0 == strncmp( TransformType, tokenType, strlen( tokenType ) ) ) {
            return TransformToken;
        } else if( 0 == strncmp( MeshType, tokenType, strlen( tokenType ) ) ) {
            return MeshToken;
        } else if( 0 == strncmp( VertexArrayType, tokenType, strlen( tokenType ) ) ) {
            return VertexArrayToken;
        } else if( 0 == strncmp( IndexArrayType, tokenType, strlen( tokenType ) ) ) {
            return IndexArrayToken;
        } else if( 0 == strncmp( MaterialType, tokenType, strlen( tokenType ) ) ) {
            return MaterialToken;
        } else if( 0 == strncmp( ColorType, tokenType, strlen( tokenType ) ) ) {
            return ColorToken;
        } else if( 0 == strncmp( TextureType, tokenType, strlen( tokenType ) ) ) {
            return TextureToken;
        }

        return NoneType;
    }

} // Namespace Grammar

namespace Assimp {
namespace OpenGEX {

USE_ODDLPARSER_NS

//------------------------------------------------------------------------------------------------
OpenGEXImporter::RefInfo::RefInfo( aiNode *node, Type type, std::vector<std::string> &names )
: m_node( node )
, m_type( type )
, m_Names( names ) {
    // empty
}

//------------------------------------------------------------------------------------------------
OpenGEXImporter::RefInfo::~RefInfo() {
    // empty
}

//------------------------------------------------------------------------------------------------
OpenGEXImporter::OpenGEXImporter() 
: m_meshCache()
, m_mesh2refMap()
, m_ctx( NULL )
, m_currentNode( NULL )
, m_nodeStack()
, m_unresolvedRefStack() {
    // empty
}

//------------------------------------------------------------------------------------------------
OpenGEXImporter::~OpenGEXImporter() {
    m_ctx = NULL;
}

//------------------------------------------------------------------------------------------------
bool OpenGEXImporter::CanRead( const std::string &file, IOSystem *pIOHandler, bool checkSig ) const {
    bool canRead( false );
    if( !checkSig ) {
        canRead = SimpleExtensionCheck( file, "ogex" );
    } else {
        static const char *token[] = { "Metric", "GeometryNode", "VertexArray (attrib", "IndexArray" };
        canRead = BaseImporter::SearchFileHeaderForToken( pIOHandler, file, token, 4 );
    }

    return canRead;
}

//------------------------------------------------------------------------------------------------
void OpenGEXImporter::InternReadFile( const std::string &filename, aiScene *pScene, IOSystem *pIOHandler ) {
    // open source file
    IOStream *file = pIOHandler->Open( filename, "rb" );
    if( !file ) {
        throw DeadlyImportError( "Failed to open file " + filename );
    }

    std::vector<char> buffer;
    TextFileToBuffer( file, buffer );

    OpenDDLParser myParser;
    myParser.setBuffer( &buffer[ 0 ], buffer.size() );
    bool success( myParser.parse() );
    if( success ) {
        m_ctx = myParser.getContext();
        pScene->mRootNode = new aiNode;
        pScene->mRootNode->mName.Set( filename );
        handleNodes( m_ctx->m_root, pScene );
    }

    resolveReferences();
}

//------------------------------------------------------------------------------------------------
const aiImporterDesc *OpenGEXImporter::GetInfo() const {
    return &desc;
}

//------------------------------------------------------------------------------------------------
void OpenGEXImporter::SetupProperties( const Importer *pImp ) {
    if( NULL == pImp ) {
        return;
    }
}

//------------------------------------------------------------------------------------------------
void OpenGEXImporter::handleNodes( DDLNode *node, aiScene *pScene ) {
    if( NULL == node ) {
        return;
    }

    DDLNode::DllNodeList childs = node->getChildNodeList();
    for( DDLNode::DllNodeList::iterator it = childs.begin(); it != childs.end(); it++ ) {
        Grammar::TokenType tokenType( Grammar::matchTokenType( ( *it )->getType().c_str() ) );
        switch( tokenType ) {
            case Grammar::MetricToken:
                handleMetricNode( *it, pScene );
                break;

            case Grammar::NameToken:
                handleNameNode( *it, pScene );
                break;

            case Grammar::ObjectRefToken:
                handleObjectRefNode( *it, pScene );
                break;

            case Grammar::MaterialRefToken:
                handleMaterialRefNode( *it, pScene );
                break;

            case Grammar::MetricKeyToken:
                break;

            case Grammar::GeometryNodeToken:
                handleGeometryNode( *it, pScene );
                break;

            case Grammar::GeometryObjectToken:
                handleGeometryObject( *it, pScene );
                break;

            case Grammar::TransformToken:
                break;

            case Grammar::MeshToken:
                break;

            case Grammar::VertexArrayToken:
                break;

            case Grammar::IndexArrayToken:
                break;

            case Grammar::MaterialToken:
                handleMaterial( *it, pScene );
                break;

            case Grammar::ColorToken:
                break;

            case Grammar::TextureToken:
                break;
            
            default:
                break;
        }
    }
}

//------------------------------------------------------------------------------------------------
void OpenGEXImporter::handleMetricNode( DDLNode *node, aiScene *pScene ) {
    if( NULL == node || NULL == m_ctx ) {
        return;
    }

    if( m_ctx->m_root != node->getParent() ) {
        return;
    }

    Property *prop( node->getProperties() );
    while( NULL != prop ) {
        if( NULL != prop->m_id ) {
            if( Value::ddl_string == prop->m_primData->m_type ) {
                std::string valName( (char*) prop->m_primData->m_data );
                int type( Grammar::isValidMetricType( valName.c_str() ) );
                if( Grammar::NoneType != type ) {
                    Value *val( node->getValue() );
                    if( NULL != val ) {
                        if( Value::ddl_float == val->m_type ) {
                            m_metrics[ type ].m_floatValue = val->getFloat();
                        } else if( Value::ddl_int32 == val->m_type ) {
                            m_metrics[ type ].m_intValue = val->getInt32();
                        } else if( Value::ddl_string == val->m_type ) {
                            m_metrics[type].m_stringValue = std::string( val->getString() );
                        } else {
                            throw DeadlyImportError( "OpenGEX: invalid data type for Metric node." );
                        }
                    }
                }
            }
        }
        prop = prop->m_next;
    }
}

//------------------------------------------------------------------------------------------------
void OpenGEXImporter::handleNameNode( DDLNode *node, aiScene *pScene ) {
    if( NULL == m_currentNode ) {
        throw DeadlyImportError( "No parent node for name." );
        return;
    }

    Value *val( node->getValue() );
    if( NULL != val ) {
        if( Value::ddl_string != val->m_type ) {
            throw DeadlyImportError( "OpenGEX: invalid data type for value in node name." );
        }

        std::string name( val->getString() );
        m_currentNode->mName.Set( name.c_str() );
    }
}

//------------------------------------------------------------------------------------------------
static void getRefNames( DDLNode *node, std::vector<std::string> &names ) {
    ai_assert( NULL != node );

    Reference *ref = node->getReferences();
    if( NULL != ref ) {
        for( size_t i = 0; i < ref->m_numRefs; i++ )  {
            Name *currentName( ref->m_referencedName[ i ] );
            if( NULL != currentName && NULL != currentName->m_id ) {
                const std::string name( currentName->m_id->m_buffer );
                if( !name.empty() ) {
                    names.push_back( name );
                }
            }
        }
    }
}

//------------------------------------------------------------------------------------------------
void OpenGEXImporter::handleObjectRefNode( DDLNode *node, aiScene *pScene ) {
    if( NULL == m_currentNode ) {
        throw DeadlyImportError( "No parent node for name." );
        return;
    }

    std::vector<std::string> objRefNames;
    getRefNames( node, objRefNames );
    m_currentNode->mNumMeshes = objRefNames.size();
    m_currentNode->mMeshes = new unsigned int[ objRefNames.size() ];
    if( !objRefNames.empty() ) {
        m_unresolvedRefStack.push_back( new RefInfo( m_currentNode, RefInfo::MeshRef, objRefNames ) );
    }
}

//------------------------------------------------------------------------------------------------
void OpenGEXImporter::handleMaterialRefNode( ODDLParser::DDLNode *node, aiScene *pScene ) {
    if( NULL == m_currentNode ) {
        throw DeadlyImportError( "No parent node for name." );
        return;
    }

    std::vector<std::string> matRefNames;
    getRefNames( node, matRefNames );
    if( !matRefNames.empty() ) {
        m_unresolvedRefStack.push_back( new RefInfo( m_currentNode, RefInfo::MaterialRef, matRefNames ) );
    }
}

//------------------------------------------------------------------------------------------------
void OpenGEXImporter::handleGeometryNode( DDLNode *node, aiScene *pScene ) {
    aiNode *newNode = new aiNode;
    pushNode( newNode, pScene );
    m_currentNode = newNode;
    handleNodes( node, pScene );
    
    popNode();
}

//------------------------------------------------------------------------------------------------
void OpenGEXImporter::handleGeometryObject( DDLNode *node, aiScene *pScene ) {
    aiMesh *currentMesh( new aiMesh );
    const size_t idx( m_meshCache.size() );
    m_meshCache.push_back( currentMesh );

    // store name to reference relation
    m_mesh2refMap[ node->getName() ] = idx;

    // todo: child nodes?

    handleNodes( node, pScene );
}

//------------------------------------------------------------------------------------------------
void OpenGEXImporter::handleMaterial( ODDLParser::DDLNode *node, aiScene *pScene ) {

}

//------------------------------------------------------------------------------------------------
void OpenGEXImporter::resolveReferences() {
    if( m_unresolvedRefStack.empty() ) {
        return;
    }

    RefInfo *currentRefInfo( NULL );
    for( std::vector<RefInfo*>::iterator it = m_unresolvedRefStack.begin(); it != m_unresolvedRefStack.end(); ++it ) {
        currentRefInfo = *it;
        if( NULL != currentRefInfo ) {
            aiNode *node( currentRefInfo->m_node );
            if( RefInfo::MeshRef == currentRefInfo->m_type ) {
                for( size_t i = 0; i < currentRefInfo->m_Names.size(); i++ ) {
                    const std::string &name(currentRefInfo->m_Names[ i ] );
                    unsigned int meshIdx = m_mesh2refMap[ name ];
                    node->mMeshes[ i ] = meshIdx;
                }
            } else if( RefInfo::MaterialRef == currentRefInfo->m_type ) {
                // ToDo
            }
        }
    }
}

//------------------------------------------------------------------------------------------------
void OpenGEXImporter::pushNode( aiNode *node, aiScene *pScene ) {
    ai_assert( NULL != pScene );

    if( NULL != node ) {
        if( m_nodeStack.empty() ) {
            node->mParent = pScene->mRootNode;
        } else {
            aiNode *parent( m_nodeStack.back() );
            ai_assert( NULL != parent );
            node->mParent = parent;
        }
        m_nodeStack.push_back( node );
    }
}

//------------------------------------------------------------------------------------------------
aiNode *OpenGEXImporter::popNode() {
    if( m_nodeStack.empty() ) {
        return NULL;
    }
    
    aiNode *node( top() );
    m_nodeStack.pop_back();
    
    return node;
}

//------------------------------------------------------------------------------------------------
aiNode *OpenGEXImporter::top() const {
    if( m_nodeStack.empty() ) {
        return NULL;
    }

    return m_nodeStack.back();
}

//------------------------------------------------------------------------------------------------
void OpenGEXImporter::clearNodeStack() {
    m_nodeStack.clear();
}

//------------------------------------------------------------------------------------------------

} // Namespace OpenGEX
} // Namespace Assimp

#endif // ASSIMP_BUILD_NO_OPENGEX_IMPORTER
