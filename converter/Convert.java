import java.io.File;
import java.util.Set;
import org.semanticweb.owlapi.apibinding.OWLManager;
import org.semanticweb.owlapi.model.OWLOntologyManager;
import org.semanticweb.owlapi.model.OWLOntology;
import org.semanticweb.owlapi.model.OWLOntologyLoaderConfiguration;
import org.semanticweb.owlapi.model.*;
import org.semanticweb.owlapi.reasoner.*;
import org.semanticweb.owlapi.io.OWLOntologyDocumentSource;
import org.semanticweb.owlapi.io.FileDocumentSource;
import org.coode.owlapi.manchesterowlsyntax.ManchesterOWLSyntaxEditorParser;
import org.semanticweb.owlapi.util.BidirectionalShortFormProvider;
import org.semanticweb.owlapi.util.BidirectionalShortFormProviderAdapter;
import org.semanticweb.owlapi.util.SimpleShortFormProvider;
import org.semanticweb.owlapi.util.AnnotationValueShortFormProvider;
import org.semanticweb.owlapi.util.OWLOntologyImportsClosureSetProvider;
import org.semanticweb.owlapi.expression.OWLEntityChecker;
import org.semanticweb.owlapi.expression.ShortFormEntityChecker;
import uk.ac.manchester.cs.owl.owlapi.OWLLiteralImplBoolean;

public class Convert
{
  OWLAnnotationProperty obsolete;

  public static void main(String [] args) throws Exception
  {
    Convert c = new Convert();
    c.run(args);
  }

  public void run(String [] args) throws Exception
  {
    OWLOntologyManager manager = OWLManager.createOWLOntologyManager();
    OWLOntologyLoaderConfiguration config = new OWLOntologyLoaderConfiguration();
    config.setMissingOntologyHeaderStrategy(OWLOntologyLoaderConfiguration.MissingOntologyHeaderStrategy.IMPORT_GRAPH);
    OWLDataFactory df = manager.getOWLDataFactory();
    obsolete = df.getOWLAnnotationProperty(IRI.create("http://www.geneontology.org/formats/oboInOwl#is_obsolete", ""));

    File kbfile;
    OWLOntology ont;

    if ( args.length != 1 )
    {
      System.err.println( "Syntax: java Convert (owlfile) >(outputfile)" );
      System.err.println( "..." );
      System.err.println( "For example: java Convert /home/ricordo/ontology/ricordo.owl >ricordo.nt" );
      return;
    }

    try
    {
      kbfile = new File(args[0]);
      ont = manager.loadOntologyFromOntologyDocument(new FileDocumentSource(kbfile),config);
    }
    catch(Exception e)
    {
      System.out.println("Load failure");
      return;
    }

    IRI iri = manager.getOntologyDocumentIRI(ont);

    Set<OWLOntology> onts = ont.getImportsClosure();

    for ( OWLOntology o : onts )
    {
      Set<OWLClass> classes = o.getClassesInSignature();

      for ( OWLClass c : classes )
      {
        if ( is_obsolete( c, o ) )
          continue;

        String cID = c.toString().trim();

        Set<OWLAnnotation> annots = c.getAnnotations(o, df.getRDFSLabel() );
        for ( OWLAnnotation a : annots )
        {
          if ( a.getValue() instanceof OWLLiteral )
            System.out.print( cID + " <http://www.w3.org/2000/01/rdf-schema#label> \"" + escape(((OWLLiteral)a.getValue()).getLiteral().trim()) + "\" .\n" );
        }
      }

      Set<OWLObjectProperty> props = o.getObjectPropertiesInSignature();

      for ( OWLObjectProperty prop : props )
      {
        if ( prop.isOWLTopObjectProperty() )
          continue;

        String pID = prop.toString().trim();

        System.out.print( pID + " <http://open-physiology.org/ont#is_predicate> \"true\" .\n" );

        Set<OWLAnnotation> annots = prop.getAnnotations(o, df.getRDFSLabel() );
        boolean fLabel = false;

        for ( OWLAnnotation a : annots )
        {
          if ( a.getValue() instanceof OWLLiteral )
          {
            System.out.print( pID + " <http://open-physiology.org/ont#predicate_label> \"" + escape(((OWLLiteral)a.getValue()).getLiteral().trim()) + "\" .\n" );
            fLabel = true;
          }
        }

        if ( !fLabel )
          System.out.print( pID + " <http://open-physiology.org/ont#predicate_label> \"" + shorturl( pID ) + "\" .\n" );
      }
    }

    return;
  }

  boolean is_obsolete( OWLClass c, OWLOntology o )
  {
    Set<OWLAnnotation> annots = c.getAnnotations(o);

    for ( OWLAnnotation a : annots )
    {
      if ( a.getProperty().getIRI().toString().equals("http://www.geneontology.org/formats/oboInOwl#is_obsolete") )
        return true;
    }

    return false;

    /*
     * The following doesn't work for some reason, todo: fix it.
     * Low priority for now though, as our reference ontologies
     * currently never explicitly say "is_obsolete false", only
     * "is_obsolete true"
     *
    Set<OWLAnnotation> annots = c.getAnnotations( o, obsolete );

    for ( OWLAnnotation a : annots )
    {
      if ( !(a.getValue() instanceof OWLLiteralImplBoolean) )
        continue;

      if ( ((OWLLiteralImplBoolean)a).parseBoolean() )
        return true;
    }

    return false;
     */
  }

  public static String shorturl(String url)
  {
    if ( url.charAt(0) == '<' )
      url = url.substring(1,url.length()-1);

    int i = url.lastIndexOf("#");

    if ( i != -1 )
      return url.substring(i+1);

    i = url.lastIndexOf("/");
    int j = url.indexOf("/");

    if ( i != j )
      return url.substring(i+1);

    return url;
  }

  String escape(String x)
  {
    StringBuilder sb;
    int i, len;

    len = x.length();
    sb = new StringBuilder(len);

    for ( i = 0; i < len; i++ )
    {
      switch( x.charAt(i) )
      {
        case '\\':
          sb.append( "\\\\" );
          break;
        case '\"':
          sb.append( "\\\"" );
          break;
        case '\n':
          sb.append( "\\n" );
          break;
        case '\r':
          sb.append( "\\r" );
          break;
        case '\t':
          sb.append( "\\t" );
          break;
        default:
          sb.append( x.charAt(i) );
          break;
      }
    }

    return sb.toString();
  }
}

