#include "Interpreteur.h"
#include <stdlib.h>
#include <iostream>
using namespace std;

Interpreteur::Interpreteur(ifstream & fichier) :
m_lecteur(fichier), m_table(), m_arbre(nullptr), nb_erreurs(0) {
}

void Interpreteur::analyse() {
    m_arbre = programme(); // on lance l'analyse de la première règle
}

void Interpreteur::tester(const string & symboleAttendu) const throw (SyntaxeException) {
    // Teste si le symbole courant est égal au symboleAttendu... Si non, lève une exception
    static char messageWhat[256];
    if (m_lecteur.getSymbole() != symboleAttendu) {
        sprintf(messageWhat,
                "Ligne %d, Colonne %d - Erreur de syntaxe - Symbole attendu : %s - Symbole trouvé : %s",
                m_lecteur.getLigne(), m_lecteur.getColonne(),
                symboleAttendu.c_str(), m_lecteur.getSymbole().getChaine().c_str());
        throw SyntaxeException(messageWhat);
    }
}

void Interpreteur::testerEtAvancer(const string & symboleAttendu) throw (SyntaxeException) {
    // Teste si le symbole courant est égal au symboleAttendu... Si oui, avance, Sinon, lève une exception
    tester(symboleAttendu);
    m_lecteur.avancer();
}

void Interpreteur::erreur(const string & message) const throw (SyntaxeException) {
    // Lève une exception contenant le message et le symbole courant trouvé
    // Utilisé lorsqu'il y a plusieurs symboles attendus possibles...
    static char messageWhat[256];
    sprintf(messageWhat,
            "Ligne %d, Colonne %d - Erreur de syntaxe - %s - Symbole trouvé : %s",
            m_lecteur.getLigne(), m_lecteur.getColonne(), message.c_str(), m_lecteur.getSymbole().getChaine().c_str());
    throw SyntaxeException(messageWhat);
}


Noeud* Interpreteur::programme() {
    // <programme> ::= procedure principale() <seqInst> finproc FIN_FICHIER
    Noeud* sequence = nullptr;
    try {
        testerEtAvancer("procedure");
    }
    catch (SyntaxeException & e) {
        this->nb_erreurs++;
        cout << "Erreur" << nb_erreurs << e.what() << endl;
        m_lecteur.avancer();
    }
    
    try {
        testerEtAvancer("principale");
    }
    catch (SyntaxeException & e) {
        this->nb_erreurs++;
        cout << "Erreur" << nb_erreurs << e.what() << endl;
        m_lecteur.avancer();
    }
    
    try {
        testerEtAvancer("(");
    }
    catch (SyntaxeException & e) {
        this->nb_erreurs++;
        cout << "Erreur" << nb_erreurs << e.what() << endl;
        m_lecteur.avancer();
    }
    
    try {
       testerEtAvancer(")"); 
    }
    catch (SyntaxeException & e) {
        this->nb_erreurs++;
        cout << "Erreur" << nb_erreurs << e.what() << endl;
        m_lecteur.avancer();
    }
    
    try {
        sequence = seqInst();
    }
    catch (SyntaxeException & e) {
        this->nb_erreurs++;
        cout << "Erreur" << nb_erreurs << e.what() << endl;
        m_lecteur.avancer();
    }

    
    try {
        testerEtAvancer("finproc");
    }
    catch (SyntaxeException & e) {
        this->nb_erreurs++;
        cout << "Erreur" << nb_erreurs << e.what() << endl;
        m_lecteur.avancer();
    }
    
    try {
        tester("<FINDEFICHIER>");
    }
    catch (SyntaxeException & e) {
        this->nb_erreurs++;
        cout << "Erreur" << nb_erreurs << e.what() << endl;
        m_lecteur.avancer();
    }

    if (nb_erreurs>0) {
        sequence = nullptr;
    }
    return sequence;
}

Noeud* Interpreteur::seqInst() {
    // <seqInst> ::= <inst> { <inst> }
    NoeudSeqInst* sequence = new NoeudSeqInst();
    do {
        sequence->ajoute(inst());
    } while (m_lecteur.getSymbole() == "<VARIABLE>" ||
            m_lecteur.getSymbole() == "si" ||
            m_lecteur.getSymbole() == "repeter" ||
            m_lecteur.getSymbole() == "tantque" ||
            m_lecteur.getSymbole() == "ecrire" ||
            m_lecteur.getSymbole() == "pour" ||
            m_lecteur.getSymbole() == "lire");
    // Tant que le symbole courant est un début possible d'instruction...
    // Il faut compléter cette condition chaque fois qu'on rajoute une nouvelle instruction
    return sequence;
}

Noeud* Interpreteur::inst() {
    // <inst> ::= <affectation>  ; | <instSi>
    if (m_lecteur.getSymbole() == "<VARIABLE>") {
        Noeud *affect = affectation();
        testerEtAvancer(";");
        return affect;
    } else if (m_lecteur.getSymbole() == "si")
        return instSi();
    else if (m_lecteur.getSymbole() == "repeter"){
        Noeud*rep = instRepeter();
        testerEtAvancer(";");
        return rep;
    }
    else if (m_lecteur.getSymbole() == "tantque") 
        return instTantQue();
    else if (m_lecteur.getSymbole() == "ecrire") {
        Noeud* ecr = instEcrire();
        testerEtAvancer(";");
        return ecr;
    } else if (m_lecteur.getSymbole() == "pour")
        return instPour();
    else if (m_lecteur.getSymbole() == "lire") {
        Noeud* lit = instLire();
        testerEtAvancer(";");
        return lit;
    }
    else erreur("Instruction incorrecte");
}

Noeud* Interpreteur::affectation() {
    // <affectation> ::= <variable> = <expression> 
    tester("<VARIABLE>");
    Noeud* var = m_table.chercheAjoute(m_lecteur.getSymbole()); // La variable est ajoutée à la table eton la mémorise
    m_lecteur.avancer();
    testerEtAvancer("=");
    Noeud* exp = expression(); // On mémorise l'expression trouvée
    return new NoeudAffectation(var, exp); // On renvoie un noeud affectation
}

Noeud* Interpreteur::expression() {
    // <expression> ::= <facteur> { <opBinaire> <facteur> }
    //  <opBinaire> ::= + | - | *  | / | < | > | <= | >= | == | != | et | ou
    Noeud* fact = facteur();
    while (m_lecteur.getSymbole() == "+" || m_lecteur.getSymbole() == "-" ||
            m_lecteur.getSymbole() == "*" || m_lecteur.getSymbole() == "/" ||
            m_lecteur.getSymbole() == "<" || m_lecteur.getSymbole() == "<=" ||
            m_lecteur.getSymbole() == ">" || m_lecteur.getSymbole() == ">=" ||
            m_lecteur.getSymbole() == "==" || m_lecteur.getSymbole() == "!=" ||
            m_lecteur.getSymbole() == "et" || m_lecteur.getSymbole() == "ou") {
        Symbole operateur = m_lecteur.getSymbole(); // On mémorise le symbole de l'opérateur
        m_lecteur.avancer();
        Noeud* factDroit = facteur(); // On mémorise l'opérande droit
        fact = new NoeudOperateurBinaire(operateur, fact, factDroit); // Et on construuit un noeud opérateur binaire
    }
    return fact; // On renvoie fact qui pointe sur la racine de l'expression
}

Noeud* Interpreteur::facteur() {
    // <facteur> ::= <entier> | <variable> | - <facteur> | non <facteur> | ( <expression> )
    Noeud* fact = nullptr;
    if (m_lecteur.getSymbole() == "<VARIABLE>" || m_lecteur.getSymbole() == "<ENTIER>") {
        fact = m_table.chercheAjoute(m_lecteur.getSymbole()); // on ajoute la variable ou l'entier à la table
        m_lecteur.avancer();
    } else if (m_lecteur.getSymbole() == "-") { // - <facteur>
        m_lecteur.avancer();
        // on représente le moins unaire (- facteur) par une soustraction binaire (0 - facteur)
        fact = new NoeudOperateurBinaire(Symbole("-"), m_table.chercheAjoute(Symbole("0")), facteur());
    } else if (m_lecteur.getSymbole() == "non") { // non <facteur>
        m_lecteur.avancer();
        // on représente le moins unaire (- facteur) par une soustractin binaire (0 - facteur)
        fact = new NoeudOperateurBinaire(Symbole("non"), facteur(), nullptr);
    } else if (m_lecteur.getSymbole() == "(") { // expression parenthésée
        m_lecteur.avancer();
        fact = expression();
        testerEtAvancer(")");
    } else
        erreur("Facteur incorrect");
    return fact;
}



Noeud* Interpreteur::instSi() {
    // <instSi> ::= si ( <expression> ) <seqInst> finsi | sinonsi (expression)
    try {
        testerEtAvancer("si");
    }    catch (SyntaxeException& e) {
        cout << "erreur : " << nb_erreurs << " " << e.what() << endl;
        nb_erreurs++;
        m_lecteur.avancer();
    }

    try {
        testerEtAvancer("(");
    }    catch (SyntaxeException& e) {
        cout << "erreur : " << nb_erreurs << " " << e.what() << endl;
        nb_erreurs++;
        m_lecteur.avancer();
    }

    Noeud* condition = expression(); // On mémorise la condition
    try {
        testerEtAvancer(")");
    }    catch (SyntaxeException& e) {
        cout << "erreur : " << nb_erreurs << " " << e.what() << endl;
        nb_erreurs++;
        m_lecteur.avancer();
    }
    Noeud* sequence = seqInst(); // On mémorise la séquence d'instruction
    NoeudInstSi* instSi = new NoeudInstSi(condition, sequence);


    while (m_lecteur.getSymbole() == "sinonsi") {
        m_lecteur.avancer();
        try {
            testerEtAvancer("(");
        }        catch (SyntaxeException& e) {
            nb_erreurs++;
            m_lecteur.avancer();
        }
        instSi->ajouterConditionSsi(expression());
        try {
            testerEtAvancer(")");
        }        catch (SyntaxeException& e) {
            nb_erreurs++;
            m_lecteur.avancer();
        }
        instSi->ajouterSequenceSsi(seqInst());
    }

    if (m_lecteur.getSymbole() == "sinon") {
        m_lecteur.avancer();
        instSi->ajouterSequenceSinon(seqInst());
    }
    try {
        testerEtAvancer("finsi");
    }    catch (SyntaxeException& e) {
        nb_erreurs++;
        m_lecteur.avancer();
    }
    return instSi; // Et on renvoie un noeud Instruction Si
}
Noeud* Interpreteur::instTantQue() {
    //<instTantQue> ::= tantque ( <expression> ) <seqInst> fintantque
    testerEtAvancer("tantque");
    testerEtAvancer("(");
    Noeud* condition = expression(); // on memorise la condition du tant que
    testerEtAvancer(")");
    Noeud* sequence = seqInst(); // on memorise la sequence d'instruction
    testerEtAvancer("fintantque");
    return new NoeudInstTantQue(condition, sequence); // on renvoie un noeud Instruction tantque
}

Noeud* Interpreteur::instRepeter() {
    // <instRepeter> ::=repeter <seqInst> jusqua( <expression> )
    try {
        testerEtAvancer("repeter");
    } catch (SyntaxeException & e) {
        cout << e.what() << endl;
        nb_erreurs++;
        m_lecteur.avancer();
    }
    Noeud* sequence = seqInst(); // On mémorise la séquence d'instruction
    try {
        testerEtAvancer("jusqua");
    } catch (SyntaxeException & e) {
        cout << e.what() << endl;
        nb_erreurs++;
        m_lecteur.avancer();
    }
    try {
        testerEtAvancer("(");
    } catch (SyntaxeException & e) {
        cout << e.what() << endl;
        nb_erreurs++;
        m_lecteur.avancer();
    }
    Noeud* condition = expression(); // On mémorise la condition
    try {
        testerEtAvancer(")");
    } catch (SyntaxeException & e) {
        cout << e.what() << endl;
        nb_erreurs++;
        m_lecteur.avancer();
    }
    return new NoeudInstRepeter(sequence, condition); // Et on renvoie un noeud Instruction Si
}

Noeud* Interpreteur::instPour() {
    // <instPour> ::= pour ( [ <affectation> ] ; <expression> ; [ <affectation> ]) <seqInst> finpour
    testerEtAvancer("pour");
    Noeud* aff1 = nullptr, *exp, *aff2 = nullptr, *seq;
    testerEtAvancer("(");
    if (m_lecteur.getSymbole() == "<VARIABLE>")
        aff1 = affectation(); // on memorise l'affectation si il y en a une 

    testerEtAvancer(";");
    exp = expression(); //on memorise l'expression
    testerEtAvancer(";");
    if (m_lecteur.getSymbole() == "<VARIABLE>")
        aff2 = affectation(); // on memorise l'affectation si il y en a une 
    testerEtAvancer(")");
    seq = seqInst(); // om memorise la sequence d'instruction
    testerEtAvancer("finpour");
    return new NoeudInstPour(aff1, exp, aff2, seq);
}

Noeud* Interpreteur::instEcrire() {
    // <instEcrire>  ::= ecrire( <expression> | <chaine> {, <expression> | <chaine> })
     try {
        testerEtAvancer("ecrire");
    } catch (SyntaxeException & e) {
        cout << e.what() << endl;
        nb_erreurs++;
        m_lecteur.avancer();
    }
    try {
        testerEtAvancer("(");
    } catch (SyntaxeException & e) {
        cout << e.what() << endl;
        nb_erreurs++;
        m_lecteur.avancer();
    }
    
    NoeudInstEcrire* ne = new NoeudInstEcrire();
    Noeud* param = nullptr;   
    if (m_lecteur.getSymbole() == "<CHAINE>") {
        param = m_table.chercheAjoute(m_lecteur.getSymbole());
        ne->ajoute(param);
        m_lecteur.avancer();
    } else {
        param = expression();
        ne->ajoute(param);
    }
    while (m_lecteur.getSymbole() == ",") {
        m_lecteur.avancer();
        if (m_lecteur.getSymbole() == "<CHAINE>") {
            param = m_table.chercheAjoute(m_lecteur.getSymbole());
            ne->ajoute(param);
            m_lecteur.avancer();
        } else {
            param = expression();
            ne->ajoute(param);
        }
    }
    testerEtAvancer(")");
    return ne;
}

Noeud* Interpreteur::instLire() {
    //<instLire> ::= lire ( <variable> { , <variable> } )
     try {
        testerEtAvancer("lire");
    } catch (SyntaxeException & e) {
        cout << e.what() << endl;
        nb_erreurs++;
        m_lecteur.avancer();
    }
    try {
        testerEtAvancer("(");
    } catch (SyntaxeException & e) {
        cout << e.what() << endl;
        nb_erreurs++;
        m_lecteur.avancer();
    }
     
     
    Noeud* param;
    Noeud* ne = new NoeudInstLire;

    if (m_lecteur.getSymbole() == "<VARIABLE>") {
        param = m_table.chercheAjoute(m_lecteur.getSymbole()); // on ajoute la variable à la table
        m_lecteur.avancer();
    }
    ne->ajoute(param);
    while (m_lecteur.getSymbole() == ",") {
        m_lecteur.avancer();
        if (m_lecteur.getSymbole() == "<VARIABLE>") {
            param = m_table.chercheAjoute(m_lecteur.getSymbole()); // on ajoute la chaine à la table
            m_lecteur.avancer();
        } else

            ne->ajoute(param);
    }
    testerEtAvancer(")");
    return ne;
}

