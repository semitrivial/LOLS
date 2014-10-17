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

public class Convert
{
  public static void main(String [] args) throws Exception
  {
    Convert c = new Convert();
    c.run();
  }

  public void run() throws Exception
  {
    OWLOntologyManager manager = OWLManager.createOWLOntologyManager();
    OWLOntologyLoaderConfiguration config = new OWLOntologyLoaderConfiguration();
    config.setMissingOntologyHeaderStrategy(OWLOntologyLoaderConfiguration.MissingOntologyHeaderStrategy.IMPORT_GRAPH);

    File kbfile;
    OWLOntology ont;

    try
    {
      kbfile = new File("/home/sarala/testkb/ricordo.owl");
      ont = manager.loadOntologyFromOntologyDocument(new FileDocumentSource(kbfile),config);
    }
    catch(Exception e)
    {
      System.out.println("Load failure");
      return;
    }

    IRI iri = manager.getOntologyDocumentIRI(ont);

    OWLDataFactory df = OWLManager.getOWLDataFactory();
    Set<OWLOntology> onts = ont.getImportsClosure();

    for ( OWLOntology o : onts )
    {
      Set<OWLClass> classes = o.getClassesInSignature();

      for ( OWLClass c : classes )
      {
        Set<OWLAnnotation> annots = c.getAnnotations(o, df.getRDFSLabel() );
        for ( OWLAnnotation a : annots )
        {
          if ( a.getValue() instanceof OWLLiteral )
            System.out.print( c.toStringID() + " " + ((OWLLiteral)a.getValue()).getLiteral() + "\n" );
        }
      }
    }

    return;
  }
}

