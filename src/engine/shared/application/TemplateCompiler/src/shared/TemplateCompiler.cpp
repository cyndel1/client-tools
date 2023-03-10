//========================================================================
//
// TemplateCompiler.cpp - entry point for templateCompiler
//
// copyright 2001 Sony Online Entertainment
//
//========================================================================

#include "FirstTemplateCompiler.h"
#include "sharedCompression/SetupSharedCompression.h"
#include "sharedDebug/SetupSharedDebug.h"
#include "sharedFile/SetupSharedFile.h"
#include "sharedFoundation/PerThreadData.h"
#include "sharedFoundation/SetupSharedFoundation.h"
#include "sharedObject/SetupSharedObject.h"
#include "sharedRandom/SetupSharedRandom.h"
#include "sharedRegex/SetupSharedRegex.h"
#include "sharedThread/SetupSharedThread.h"
#include "sharedUtility/TemplateParameter.h"

#include "sharedTemplateDefinition/File.h"
#include "sharedTemplateDefinition/ObjectTemplate.h"
#include "sharedTemplateDefinition/TemplateDefinitionFile.h"
#include "sharedTemplateDefinition/TemplateData.h"
#include "sharedTemplateDefinition/TemplateGlobals.h"
#include "sharedTemplateDefinition/TpfFile.h"

#include "sharedTemplate/SetupSharedTemplate.h"

#pragma warning (disable:4100) // unreferenced formal parameters abound in the perforce clientAPI
#include "clientapi.h"
#pragma warning (default:4100)

#include <ctime>


//==============================================================================
// subclass Perforce API class ClientUser in order to trap errors

static const int SUBMIT_NO_FILE_ERR = 17;	// need to add file before submitting

class MyPerforceUser : public ClientUser
{
public:
	MyPerforceUser(void) : ClientUser(), m_errorOccurred(false) {}
  virtual ~MyPerforceUser() {}
	virtual void HandleError( Error *err )
	{
		if (err != NULL && err->Test())
		{
			m_errorOccurred = true;
			m_lastError = err->GetGeneric();
			// test for filtered errors
			for (size_t i = 0; i < m_filteredErrors.size(); ++i)
			{
				if (m_lastError == m_filteredErrors[i])
					return;
			}
		}
		ClientUser::HandleError(err);
	}

	bool errorOccurred(void) const
	{
		return m_errorOccurred;
	}

	int getLastError(void) const
	{
		return m_lastError;
	}

	void clearLastError(void)
	{
		m_errorOccurred = false;
		m_lastError = 0;
	}

	void addFilteredError(int error)
	{
		m_filteredErrors.push_back(error);
	}

	void clearFilteredErrors(void)
	{
		m_filteredErrors.clear();
	}

private:
	bool m_errorOccurred;
	int m_lastError;
	std::vector<int> m_filteredErrors;
};

//==============================================================================
// subclass of the PerforceAPI StrBuf class, to workaround a bug
//  in the destructor. We can't fix the bug because it's an external library
class StrBufFixed : public StrBuf
{
public:
	~StrBufFixed()
	{
		delete buffer;
		StringInit();
	}
};



//==============================================================================
// functions

/**
 * Appends an extension to the end of a filename.
 *
 * @param filename		the failename to append to
 * @param extension		the extension to extend
 */
void appendExtension(std::string & filename, const char *extension)
{
	// see if the filename already has the extension
	std::string::size_type matchPos = filename.rfind(extension);
	if (matchPos != std::string::npos && filename[matchPos - 1] == '.')
		return;
	filename.append(".");
	filename.append(extension);
}	// appendExtension

/**
 * Creates a default template file from a template definition file.
 *
 * @param definitionFp		template definition file to read from
 * @param templateFile		template file to write to
 *
 * @return 0 on success, error code on fail
 */
int generateTemplate(File &definitionFp, File &templateFp)
{
	// parse the template definition file
	TemplateDefinitionFile TemplateDefinitionFile;
	int result = TemplateDefinitionFile.parse(definitionFp);
	if (result != 0)
		return result;

	// get the latest version
	const TemplateData *currentTemplate = TemplateDefinitionFile.getTemplateData(
		TemplateDefinitionFile.getHighestVersion());
	NOT_NULL(currentTemplate);

	templateFp.print("@class %s %d\n\n", TemplateDefinitionFile.getTemplateFilename().c_str(),
		TemplateDefinitionFile.getHighestVersion());

	currentTemplate->writeDefaultTemplateFile(templateFp);

	// we now need to move down the template definition heiarchy and write the
	// parameters of the base class
	Filename basename(NULL, definitionFp.getFilename().getPath().c_str(),
		TemplateDefinitionFile.getBaseFilename().c_str(), TEMPLATE_DEFINITION_EXTENSION);
	if (basename.getName().size() == 0)
		return 0;

//	basename.setExtension(TEMPLATE_DEFINITION_EXTENSION);
	File baseFp;
	if (!baseFp.open(basename, "rt"))
	{
		fprintf(stderr, "Cannot open template definition file %s\n",
			baseFp.getFilename().getFullFilename().c_str());
		return -1;
	}

	templateFp.print("\n\n");

	result = generateTemplate(baseFp, templateFp);
	return result;
}	// generateTemplate(File &, File &)

/**
 * Creates a default template file from a template definition file.
 *
 * @param definitionFile		the filename of the definition
 * @param templateFile			the filename of the template
 *
 * @return 0 on success, error code on fail
 */
int generateTemplate(const char *definitionFile, const char *templateFile)
{
	// check filename extensions
	Filename defFile(NULL, NULL, definitionFile, TEMPLATE_DEFINITION_EXTENSION);
	Filename temFile(NULL, NULL, templateFile, TEMPLATE_EXTENSION);

	File definitionFp;
	int i = 0;
	while (!definitionFp.exists(defFile) && i < MAX_DIRECTORY_DEPTH)
	{
		defFile.appendPath(NEXT_HIGHER_PATH);
		++i;
	}
	if (i == MAX_DIRECTORY_DEPTH)
	{
		fprintf(stderr, "Cannot find template definition file %s\n",
			defFile.getName().c_str());
		return -1;
	}
	if (!definitionFp.open(defFile, "rt"))
	{
		fprintf(stderr, "Cannot open template definition file %s\n",
			defFile.getName().c_str());
		return -1;
	}

	File templateFp;
	if (!templateFp.open(temFile, "wt"))
	{
		fprintf(stderr, "Cannot open template file %s\n",
			templateFp.getFilename().getFullFilename().c_str());
		return -1;
	}

	int result = generateTemplate(definitionFp, templateFp);
	return result;
}	// generateTemplate(const char *, const char *)

/**
 * Derives a default template file from another template file.
 *
 * @param baseFile			the filename of the source template
 * @param templateFile		the filename of the destination template
 *
 * @return 0 on success, error code on fail
 */
int deriveTemplate(const char *baseFile, const char *templateFile)
{
	// check filename extensions
	Filename basFile(NULL, NULL, baseFile, TEMPLATE_EXTENSION);
	Filename temFile(NULL, NULL, templateFile, TEMPLATE_EXTENSION);

	File basFp;
	if (!basFp.open(basFile, "rt"))
	{
		fprintf(stderr, "Cannot open base template file %s\n",
			basFp.getFilename().getFullFilename().c_str());
		return -1;
	}
	File temFp;
	if (!temFp.open(temFile, "wt"))
	{
		fprintf(stderr, "Cannot open derived template file %s\n",
			temFp.getFilename().getFullFilename().c_str());
		return -1;
	}

	return 0;
}	// deriveTemplate

/**
 * Compiles a template file into an iff file.
 *
 * @param filename		the filename of the template
 *
 * @return 0 on success, error code on fail
 */
int compileTemplate(const char *filename)
{
TpfFile templateFile;

	Filename templateFileName(NULL, NULL, filename, TEMPLATE_EXTENSION);
	return templateFile.makeIffFiles(templateFileName);
}	// compileTemplate

/**
 * Tests to see if a template file is valid. Does not generate an iff file.
 *
 * @param filename		the filename of the template
 *
 * @return 0 on success, error code on fail
 */
int verifyTemplate(const char *filename)
{
TpfFile templateFile;

	printf("Verifying %s: ", filename);
	Filename templateFileName(NULL, NULL, filename, TEMPLATE_EXTENSION);
	int result = templateFile.loadTemplate(templateFileName);
	if (result == 0)
		printf("file ok");
	printf("\n");
	return result;
}	// verifyTemplate

/**
 * Adds or removes parameters from a template based on the current template
 * definition.
 *
 * @param filename		the filename of the template
 *
 * @return 0 on success, error code on fail
 *
int updateTemplate(const char *filename)
{
TpfFile templateFile;

	Filename templateFileName(NULL, NULL, filename, TEMPLATE_EXTENSION);
	return templateFile.updateTemplate(templateFileName);
}	// updateTemplate
*/
/**
 * Checks out a template file and the iff files associated with it from Perforce.
 *
 * @param filename		the filename of the template
 *
 * @return 0 on success, error code on fail
 */
int checkOut(const char *filename)
{
MyPerforceUser ui;
ClientApi client;
Error e;

	// check filename extensions
	Filename templateFileName(NULL, NULL, filename, TEMPLATE_EXTENSION);
	Filename iffFileName = templateFileName;
	iffFileName.setExtension(IFF_EXTENSION);

	// Connect to Perforce server
	client.Init( &e );
	if (e.Test())
	{
		StrBufFixed msg;
		e.Fmt(&msg);
		fprintf(stderr, msg.Text());
		return -1;
	}

	// check out the template file
	const char * commands[2];
	commands[0] = "edit";
	commands[1] = templateFileName;
	client.SetArgv( 1, const_cast<char **>(&commands[1]) );
	client.Run( commands[0], &ui );
	if (ui.errorOccurred())
		return -1;

	// find the client and server paths
	TpfFile templateFile;
	IGNORE_RETURN(templateFile.loadTemplate(templateFileName));

	// check out the client template iff file
	Filename iffName(NULL, templateFile.getIffPath().c_str(), iffFileName, IFF_EXTENSION);
	commands[1] = iffName;
	client.SetArgv( 1, const_cast<char **>(&commands[1]) );
	client.Run( commands[0], &ui );
	if (ui.errorOccurred())
		return -1;

	// Close connection
	return client.Final( &e );
}	// checkOut

/**
 * Checks in a template file and the iff files associated with it to Perforce.
 *
 * @param filename		the filename of the template
 *
 * @return 0 on success, error code on fail
 */
int checkIn(const char *filename)
{
MyPerforceUser ui;
ClientApi client;
Error e;

	// check filename extensions
	Filename templateFileName(NULL, NULL, filename, TEMPLATE_EXTENSION);
	Filename iffFileName = templateFileName;
	iffFileName.setExtension(IFF_EXTENSION);

	// find the client and server paths
	TpfFile templateFile;
	int result = templateFile.loadTemplate(templateFileName);
	if (result != 0)
	{
		// don't allow check-in if there are errors
		return result;
	}

	// Connect to Perforce server
	client.Init( &e );
	if (e.Test())
	{
		StrBufFixed msg;
		e.Fmt(&msg);
		fprintf(stderr, msg.Text());
		return -1;
	}

	// try to submit the files
	const char * commands[4];
	char param1[256];
	for (;;)
	{
		sprintf(param1, "//depot/.../%s.*", templateFileName.getName().c_str());
		commands[0] = "submit";
		commands[1] = param1;

		// don't report an error if the files need to be added before submitting
		ui.addFilteredError(SUBMIT_NO_FILE_ERR);
		client.SetArgv( 1, const_cast<char **>(&commands[1]) );
		client.Run( commands[0], &ui );
		if (!ui.errorOccurred())
			break;
		if (ui.getLastError() != SUBMIT_NO_FILE_ERR)
			return -1;
		ui.clearLastError();
		ui.clearFilteredErrors();

		// we need to add the files to Perforce before submitting
		commands[0] = "add";

		// add the template file
		commands[1] = templateFileName;
		client.SetArgv( 1, const_cast<char **>(&commands[1]) );
		client.Run( commands[0], &ui );
		if (ui.errorOccurred())
			return -1;

		// add the client iff file
		Filename iffName(NULL, templateFile.getIffPath().c_str(), iffFileName, NULL);
		commands[1] = iffName;
		client.SetArgv( 1, const_cast<char **>(&commands[1]) );
		client.Run( commands[0], &ui );
		if (ui.errorOccurred())
			return -1;
	}

	// Close connection
	return client.Final( &e );
}	// checkIn

/**
 * Prints the command syntax to the console.
 */
void printSyntax(void)
{
	printf("TemplateCompiler " __DATE__ " " __TIME__ "\n\n");
	printf("Compiler commands:\n");
	printf("-generate <defname>[.tdf] <templatename>[.tpf]\n");
//	printf("-update <filename1>[.tpf] [<filename2>[.tpf] ...]\n");
//	printf("-derive <basename>[.tpf] <derivedname>[.tpf]\n");
	printf("-compile <filename1>[.tpf] [<filename2>[.tpf] ...]\n");
	printf("-verify <filename1>[.tpf] [<filename2>[.tpf] ...]\n");
	printf("Perforce commands:\n");
	printf("-edit <filename1>[.tpf] [<filename2>[.tpf] ...]\n");
	printf("-submit <filename1>[.tpf] [<filename2>[.tpf] ...]\n");
}	// printSyntax

/**
 * Processes the args sent to main.
 */
int processArgs(int argc, char *argv[ ])
{
	if (argc < 3)
	{
		printSyntax();
		return 0;
	}

	if (strcmp(argv[1], "-generate") == 0)
	{
		if (argc != 4)
		{
			printSyntax();
			return 0;
		}
		return generateTemplate(argv[2], argv[3]);
	}
//	else if (strcmp(argv[1], "-update") == 0)
//	{
//		if (argc < 3)
//		{
//			printSyntax();
//			return 0;
//		}
//		for (int i = 2; i < argc; ++i)
//		{
//			int result = updateTemplate(argv[i]);
//			if (result != 0)
//				return result;
//		}
//	}
	else if (strcmp(argv[1], "-derive") == 0)
	{
		if (argc != 4)
		{
			printSyntax();
			return 0;
		}
		return deriveTemplate(argv[2], argv[3]);
	}
	else if (strcmp(argv[1], "-compile") == 0)
	{
		if (argc < 3)
		{
			printSyntax();
			return 0;
		}
		for (int i = 2; i < argc; ++i)
		{
			int result = compileTemplate(argv[i]);
			if (result != 0)
				return result;
		}
	}
	else if (strcmp(argv[1], "-compileeditor") == 0)
	{
		if (argc < 3)
		{
			printSyntax();
			return 0;
		}
		for (int i = 2; i < argc; ++i)
		{
			int result = compileTemplate(argv[i]);
			if (result != 0)
				return result;
		}

		printf ("%s compiled successfully.\n", argv [2]);
	}
	else if (strcmp(argv[1], "-verify") == 0)
	{
		if (argc < 3)
		{
			printSyntax();
			return 0;
		}
		int result = 0;
		for (int i = 2; i < argc; ++i)
		{
			int tempResult = verifyTemplate(argv[i]);
			if (tempResult != 0)
				result = tempResult;
		}
		return result;
	}
	else if (strcmp(argv[1], "-edit") == 0)
	{
		if (argc < 3)
		{
			printSyntax();
			return 0;
		}
		for (int i = 2; i < argc; ++i)
		{
			int result = checkOut(argv[i]);
			if (result != 0)
				return result;
		}
	}
	else if (strcmp(argv[1], "-submit") == 0)
	{
		if (argc < 3)
		{
			printSyntax();
			return 0;
		}
		for (int i = 2; i < argc; ++i)
		{
			int result = checkIn(argv[i]);
			if (result != 0)
				return result;
		}
	}
	else
	{
		printSyntax();
		return 0;
	}

	return 0;
}	// processArgs

/**
 * Program entry point.
 */
int main(int argc, char *argv[ ])
{
	SetupSharedThread::install();
	SetupSharedDebug::install(4096);

	{
		SetupSharedFoundation::Data data(SetupSharedFoundation::Data::D_console);
#ifdef WIN32
		char buffer[1024];
		GetModuleFileName(GetModuleHandle(NULL), buffer, 1024);
		Filename configName;
		configName.setName(buffer);
		configName.setName("templateCompiler.cfg");
		data.configFile = configName.getFullFilename().c_str();
#endif
		SetupSharedFoundation::install (data);
	}

	SetupSharedRegex::install();

	SetupSharedCompression::install();
	SetupSharedFile::install(false);

	// setup the random number generator
	// @todo need a better seed
	SetupSharedRandom::install(static_cast<uint32>(time(NULL)));

	// install templates
	SetupSharedTemplate::install();

#ifdef WIN32
	// find out what platform we are running on
	DWORD version = GetVersion();
	if (version & 0x80000000)
		WindowsUnicode = false;
	else
		WindowsUnicode = true;
#endif

	int result = processArgs(argc, argv);

	// cleanup
	SetupSharedFoundation::remove();
	PerThreadData::threadRemove();

	return result;
}	// main


// ======================================================================
