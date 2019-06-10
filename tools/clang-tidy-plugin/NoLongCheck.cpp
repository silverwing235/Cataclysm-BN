#include "NoLongCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

using namespace clang::ast_matchers;

namespace clang
{
namespace tidy
{
namespace cata
{

void NoLongCheck::registerMatchers( MatchFinder *Finder )
{
    using TypeMatcher = clang::ast_matchers::internal::Matcher<QualType>;
    const TypeMatcher isIntegerOrRef = anyOf( isInteger(), references( isInteger() ) );
    Finder->addMatcher( valueDecl( hasType( isIntegerOrRef ) ).bind( "decl" ), this );
    Finder->addMatcher( functionDecl( returns( isIntegerOrRef ) ).bind( "return" ), this );
    Finder->addMatcher( cxxStaticCastExpr( hasDestinationType( isIntegerOrRef ) ).bind( "cast" ),
                        this );
}

static std::string AlternativesFor( QualType Type )
{
    Type = Type.getNonReferenceType();
    Type = Type.getLocalUnqualifiedType();
    std::string name = Type.getAsString();
    if( name == "long" ) {
        return "Prefer int or int64_t to long";
    } else if( name == "unsigned long" ) {
        return "Prefer unsigned int or uint64_t to unsigned long";
    } else {
        return {};
    }
}

static void CheckDecl( NoLongCheck &Check, const MatchFinder::MatchResult &Result )
{
    const ValueDecl *MatchedDecl = Result.Nodes.getNodeAs<ValueDecl>( "decl" );
    if( !MatchedDecl || !MatchedDecl->getLocation().isValid() ) {
        return;
    }
    QualType Type = MatchedDecl->getType();
    std::string alternatives = AlternativesFor( Type );
    if( alternatives.empty() ) {
        return;
    }
    Decl::Kind contextKind = MatchedDecl->getDeclContext()->getDeclKind();
    if( contextKind == Decl::Function || contextKind == Decl::CXXMethod ||
        contextKind == Decl::CXXConstructor || contextKind == Decl::CXXConversion ||
        contextKind == Decl::CXXDestructor || contextKind == Decl::CXXDeductionGuide ) {
        TemplateSpecializationKind tsk =
            static_cast<const FunctionDecl *>(
                MatchedDecl->getDeclContext() )->getTemplateSpecializationKind();
        if( tsk == TSK_ImplicitInstantiation ) {
            // This happens for e.g. a parameter 'T a' to an instantiated
            // template function where T is long.  We don't want to report such
            // cases.
            return;
        }
    }
    Check.diag(
        MatchedDecl->getLocation(), "Variable %0 declared as %1.  %2." ) <<
                MatchedDecl << Type << alternatives;
}

static void CheckReturn( NoLongCheck &Check, const MatchFinder::MatchResult &Result )
{
    const FunctionDecl *MatchedDecl = Result.Nodes.getNodeAs<FunctionDecl>( "return" );
    if( !MatchedDecl || !MatchedDecl->getLocation().isValid() ) {
        return;
    }
    QualType Type = MatchedDecl->getReturnType();
    std::string alternatives = AlternativesFor( Type );
    if( alternatives.empty() ) {
        return;
    }
    Check.diag(
        MatchedDecl->getLocation(), "Function %0 declared as returning %1.  %2." ) <<
                MatchedDecl << Type << alternatives;
}

static void CheckCast( NoLongCheck &Check, const MatchFinder::MatchResult &Result )
{
    const CXXStaticCastExpr *MatchedDecl = Result.Nodes.getNodeAs<CXXStaticCastExpr>( "cast" );
    if( !MatchedDecl ) {
        return;
    }
    QualType Type = MatchedDecl->getType();
    std::string alternatives = AlternativesFor( Type );
    if( alternatives.empty() ) {
        return;
    }
    SourceLocation location = MatchedDecl->getTypeInfoAsWritten()->getTypeLoc().getBeginLoc();
    Check.diag( location, "Static cast to %0.  %1." ) << Type << alternatives;
}

void NoLongCheck::check( const MatchFinder::MatchResult &Result )
{
    CheckDecl( *this, Result );
    CheckReturn( *this, Result );
    CheckCast( *this, Result );
}

} // namespace cata
} // namespace tidy
} // namespace clang
