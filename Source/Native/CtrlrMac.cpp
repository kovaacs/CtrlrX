#include "stdafx.h"
#include "stdafx_luabind.h"
static const int zero=0;
#ifdef __APPLE__
#include <memory>
#include "CtrlrPanel/CtrlrPanel.h"
#include "CtrlrMac.h"
#include "CtrlrMacros.h"
#include "CtrlrLog.h"

#include <random>
#include <fstream>


CtrlrMac::CtrlrMac(CtrlrManager &_owner) : owner(_owner)
{
}

CtrlrMac::~CtrlrMac()
{
}

const Result CtrlrMac::exportWithDefaultPanel(CtrlrPanel* panelToWrite, const bool isRestricted, const bool signPanel) {
    if (panelToWrite == nullptr) {
        return (Result::fail("MAC native, panel pointer is invalid"));
    }
    
    File me = File::getSpecialLocation(File::currentApplicationFile);
    File newMe;
    MemoryBlock panelExportData, panelResourcesData;
    String error;
    
    // Define FileChooser and source file name to clone and mod as output file
    fc = std::make_unique<FileChooser> (CTRLR_NEW_INSTANCE_DIALOG_TITLE,
                                        me.getParentDirectory().getChildFile(File::createLegalFileName(panelToWrite->getProperty(Ids::name))).withFileExtension(me.getFileExtension()),
                                        me.getFileExtension(),
                                        panelToWrite->getOwner().getProperty(Ids::ctrlrNativeFileDialogs));
    
    // Launch FileChooser to export file and define the new output file name and extension
    if (fc->browseForDirectory()) {
        newMe = fc->getResult().getChildFile(File::createLegalFileName(panelToWrite->getProperty(Ids::name).toString() + me.getFileExtension()));
        if (!me.copyDirectoryTo(newMe)) {
            return (Result::fail("MAC native, copyDirectoryTo from \"" + me.getFullPathName() + "\" to \"" + newMe.getFullPathName() + "\" failed"));
        }
    } else {
        return (Result::fail("MAC native, browse for directory dialog failed"));
    }
    
    
    Result res = setBundleInfo(panelToWrite, newMe);
    if (!res.wasOk()) {
        return (res);
    }
    
    res = setBundleInfoCarbon(panelToWrite, newMe);
    if (!res.wasOk()) {
        return (res);
    }
    
    if ((error = CtrlrPanel::exportPanel(panelToWrite, File(), newMe, &panelExportData, &panelResourcesData, isRestricted)) == "") {
        File panelFile = newMe.getChildFile("Contents/Resources/" + String(CTRLR_MAC_PANEL_FILE));
        File resourcesFile = newMe.getChildFile("Contents/Resources/" + String(CTRLR_MAC_RESOURCES_FILE));
        File fileEncrypted = newMe.getChildFile("Contents/Resources/"+String(CTRLR_MAC_PANEL_FILE)+String("BF")); // Added v5.6.31
        
        if (panelFile.create() && panelFile.hasWriteAccess()){
            if (!panelFile.replaceWithData(panelExportData.getData(), panelExportData.getSize()))
            {
                return (Result::fail("MAC native, failed to write panel file at: " + panelFile.getFullPathName()));
            }
        }
        
        if (resourcesFile.create() && resourcesFile.hasWriteAccess())
        {
            if (!resourcesFile.replaceWithData(panelResourcesData.getData(), panelResourcesData.getSize()))
            {
                return (Result::fail("MAC native, failed to write resources file at: " + resourcesFile.getFullPathName()));
            }
        }
        
        // Encrypt the Gzipped panel file as a new Blowfish encrypyted derived file. Added v5.6.31
        if (panelFile.existsAsFile()) {
            // Read file contents into a MemoryBlock
            MemoryBlock dataToEncrypt;
            if (!panelFile.loadFileAsData(dataToEncrypt)) {
                // Handle error: Failed to read source file
                return Result::fail("Error: Failed to read source file");
            }
            
            // Define the BlowFish encryption key as string
            String keyString = "yourkey"; // Replace with your actual key (security!) Added v5.6.31
            
            // Key is provided, proceed with encryption
            BlowFish blowfish(keyString.toUTF8(), keyString.getNumBytesAsUTF8());
            
            // Encrypt the data in-place (modifies the original file)
            blowfish.encrypt(dataToEncrypt);
            
            // Create the encrypted file
            fileEncrypted.create();
            if (!fileEncrypted.existsAsFile()) {
                // Handle error: Failed to create encrypted file
                return Result::fail("Error: Failed to create encrypted file");
            }
            
            // Open encrypted file for writing
            FileOutputStream fos(fileEncrypted);
            if (!fos.openedOk()) {
                // Handle error: Failed to open encrypted file for writing
                return Result::fail("Error: Failed to open encrypted file");
            }
            
            // Write encrypted data to the file
            fos.write(dataToEncrypt.getData(), dataToEncrypt.getSize());
            fos.flush();
            
            // Encryption successful, delete the source file
            if (!panelFile.deleteFile()) {
                return Result::fail("Failed to delete source file " + panelFile.getFullPathName());
            }
        }
    }
    
    return Result::ok();
}

const Result CtrlrMac::getDefaultPanel(MemoryBlock &dataToWrite) {

    File me = File::getSpecialLocation(File::currentApplicationFile).getChildFile("Contents/Resources/"+String(CTRLR_MAC_PANEL_FILE));
    File meBF = File::getSpecialLocation(File::currentApplicationFile).getChildFile("Contents/Resources/"+String(CTRLR_MAC_PANEL_FILE)+String("BF"));
    
    _DBG("MAC native, loading panel BF from file: \""+me.getFullPathName()+"\"");
    
    // Try loading the encrypted file first
    if (meBF.existsAsFile()) {
        // Decrypt file "meBF" (panelZBF)
        MemoryBlock encryptedData;
        if (!meBF.loadFileAsData(encryptedData)) {
            return Result::fail("Error reading encrypted file \"" + meBF.getFullPathName() + "\"");
        }
        
        // Define the BlowFish encryption key as string
        String keyString = "yourkey"; // Replace with your actual key (security!). Added v5.6.31
        
        // Check if a blowfish key is provided
        if (keyString.isEmpty()) {
            return Result::fail("Blowfish key not provided for decryption");
        } else {
            // Key is provided, proceed with encryption
            BlowFish blowfish(keyString.toUTF8(), keyString.getNumBytesAsUTF8());
            blowfish.decrypt(encryptedData.getData(), encryptedData.getSize());
        }
        
        // Decrypted data is already in 'data', append to output. Added v5.6.31
        dataToWrite.append(encryptedData.getData(), encryptedData.getSize());
        
        return Result::ok();
        
    } else {
        // Fallback to panelZ if panelZBF doesn't exist
        _DBG("MAC native, \"" + meBF.getFullPathName() + "\" does not exist, falling back to \"" + me.getFullPathName() + "\"");
        
        // Read "me" (panelZ) file contents directly
        if (me.existsAsFile()){
            // File "me" (panelZ) loaded successfully, treat it as plain data
            me.loadFileAsData (dataToWrite);
            return (Result::ok());
        } else {
            // Error loading "me" (panelZ) file
            return Result::fail("MAC native, failed to load panel from \"" + me.getFullPathName() + "\"");
        }
    }
}


const Result CtrlrMac::getDefaultResources(MemoryBlock& dataToWrite)
{
#ifdef DEBUG_INSTANCE
	File meRes = File("/Users/atom/devel/debug.z");
#else
	File meRes = File::getSpecialLocation(File::currentApplicationFile).getChildFile("Contents/Resources/"+String(CTRLR_MAC_RESOURCES_FILE));
#endif
	_DBG("MAC native, loading resuources from: \""+meRes.getFullPathName()+"\"");

	if (meRes.existsAsFile())
	{
		meRes.loadFileAsData (dataToWrite);
		return (Result::ok());
	}

	return (Result::fail("MAC native, \""+meRes.getFullPathName()+"\" does not exist"));
}

const Result CtrlrMac::setBundleInfo(CtrlrPanel *sourceInfo, const File &bundle)
{
	File plist = bundle.getChildFile("Contents/Info.plist");

	if (plist.existsAsFile() && plist.hasWriteAccess())
	{
		std::unique_ptr <XmlElement> plistXml (XmlDocument::parse(plist));
		if (plistXml == nullptr)
		{
			return (Result::fail("MAC native, can't parse Info.plist as a XML document"));
		}

		XmlElement *dict = plistXml->getChildByName("dict");
        XmlElement *cfInsertKeyManufacturerID = dict->createNewChildElement("key");
        cfInsertKeyManufacturerID->addTextElement("ManufacturerID");
        XmlElement *cfInsertStringManufacturerID = dict->createNewChildElement("string");
        cfInsertStringManufacturerID->addTextElement("ManufacturerID");
        
		if (dict != nullptr)
		{
			forEachXmlChildElement (*dict, e1)
			{
				if (e1->hasTagName("key") && e1->getAllSubText() == "CFBundleName")
				{
					XmlElement *cfBundleElement = e1->getNextElementWithTagName("string");
					if (cfBundleElement != nullptr)
					{
						cfBundleElement->deleteAllTextElements();
						cfBundleElement->addTextElement(sourceInfo->getProperty(Ids::name).toString());
					}
				}
				if (e1->hasTagName("key") && (e1->getAllSubText() == "CFBundleShortVersionString" || e1->getAllSubText() == "CFBundleVersion"))
				{
					XmlElement *cfVersionElement = e1->getNextElementWithTagName("string");
					if (cfVersionElement != nullptr)
					{
						cfVersionElement->deleteAllTextElements();
						cfVersionElement->addTextElement(sourceInfo->getVersionString(false,false,"."));
					}
				}
                if (e1->hasTagName("key") && (e1->getAllSubText() == "CFBundleSignature"))
                {
                    XmlElement *cfVersionElement = e1->getNextElementWithTagName("string");
                    if (cfVersionElement != nullptr)
                    {
                        cfVersionElement->deleteAllTextElements();
                        cfVersionElement->addTextElement(sourceInfo->getProperty(Ids::panelInstanceUID).toString());
                    }
                }
                if (e1->hasTagName("key") && (e1->getAllSubText() == "NSHumanReadableCopyright"))
				{
					XmlElement *nsCopyright = e1->getNextElementWithTagName("string");
					if (nsCopyright != nullptr)
					{
						nsCopyright->deleteAllTextElements();
						nsCopyright->addTextElement(sourceInfo->getProperty(Ids::panelAuthorName).toString());
					}
                }
                if (e1->hasTagName("key") && (e1->getAllSubText() == "ManufacturerID"))
                {
                    XmlElement *cfManufacturerID = e1->getNextElementWithTagName("string");
                    if (cfManufacturerID != nullptr)
                    {
                        cfManufacturerID->deleteAllTextElements();
                        cfManufacturerID->addTextElement(sourceInfo->getProperty(Ids::panelInstanceManufacturerID).toString());
                    }
                }
                if (e1->hasTagName ("key") && (e1->getAllSubText() == "AudioComponents"))
                {
                    _DBG("INSTANCE: AudioComponents found");

                    /* resource section */
                    XmlElement *dict = nullptr;
                    XmlElement *array = e1->getNextElement();
                    if (array)
                    {
                        _DBG("INSTANCE: array is valid");
                        dict = array->getChildByName("dict");
                    }

                    if (dict != nullptr)
                    {
                        _DBG("INSTANCE: dict is valid");

                        forEachXmlChildElement (*dict, e2)
                        {
                            _DBG("INSTANCE: enum element: "+e2->getTagName());
                            _DBG("INSTANCE: enum subtext: "+e2->getAllSubText());
                            if (e2->hasTagName("key") && (e2->getAllSubText() == "description"))
                            {
                                _DBG("\tmodify");
                                XmlElement *description = e2->getNextElementWithTagName("string");
                                if (description != nullptr)
                                {
                                    description->deleteAllTextElements();
                                    description->addTextElement (sourceInfo->getProperty(Ids::name).toString());
                                }
                            }

                            if (e2->hasTagName("key") && (e2->getAllSubText() == "manufacturer"))
                            {
                                _DBG("\tmodify");
                                XmlElement *manufacturer = e2->getNextElementWithTagName("string");
                                if (manufacturer != nullptr)
                                {
                                    manufacturer->deleteAllTextElements();
                                    manufacturer->addTextElement(sourceInfo->getProperty(Ids::panelInstanceManufacturerID).toString().isEmpty() ? generateRandomUniquePluginId() : sourceInfo->getProperty(Ids::panelInstanceManufacturerID).toString());
                                }
                            }

                            if (e2->hasTagName("key") && (e2->getAllSubText() == "name"))
                            {
                                XmlElement *name = e2->getNextElementWithTagName("string");
                                if (name != nullptr)
                                {
                                    name->deleteAllTextElements();
                                    name->addTextElement(sourceInfo->getProperty(Ids::panelAuthorName).toString() + ": " + sourceInfo->getProperty(Ids::name).toString());
                                }
                            }

                            if (e2->hasTagName("key") && (e2->getAllSubText() == "subtype"))
                            {
                                _DBG("\tmodify");
                                XmlElement *subtype = e2->getNextElementWithTagName("string");
                                if (subtype != nullptr)
                                {
                                    subtype->deleteAllTextElements();
                                    subtype->addTextElement(sourceInfo->getProperty(Ids::panelInstanceUID).toString().isEmpty() ? generateRandomUniquePluginId() : sourceInfo->getProperty(Ids::panelInstanceUID).toString());
                                }
                            }

                            if (e2->hasTagName("key") && (e2->getAllSubText() == "version"))
                            {
                                _DBG("\tmodify");
                                XmlElement *version = e2->getNextElementWithTagName("integer");
                                if (version != nullptr)
                                {
                                    version->deleteAllTextElements();
                                    version->addTextElement (_STR(getVersionAsHexInteger (sourceInfo->getVersionString(false,false,"."))));
                                }
                            }
                        }
                    }
                }
			}
		}
		else
		{
			return (Result::fail("MAC native, Info.plist does not contain <dict /> element"));
		}
		plist.replaceWithText(plistXml->createDocument("<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">"));
        
		return (Result::ok());
	}
	else
	{
		 return (Result::fail("MAC native, Info.plist does not exist or is not writable: \""+plist.getFullPathName()+"\""));
    }

    return (Result::ok());
}

const Result CtrlrMac::setBundleInfoCarbon (CtrlrPanel *sourceInfo, const File &bundle)
{
#ifdef JUCE_DEBUG
	File rsrcFile = bundle.getChildFile ("Contents/Resources/Ctrlr-Debug.rsrc");
#else
	File rsrcFile = bundle.getChildFile ("Contents/Resources/Ctrlr.rsrc");
#endif

    const String instanceName           = sourceInfo->getPanelInstanceName();
    const String instanceManufacturer   = sourceInfo->getPanelInstanceManufacturer();
    const String instanceID             = sourceInfo->getPanelInstanceID();
    const String instanceManufacturerID = sourceInfo->getPanelInstanceManufacturerID();

    String nameToWrite			= String (instanceManufacturer + ": " + instanceName).substring (0,127);
    const String idToWrite      = instanceID+instanceManufacturerID;
	const int nameLength		= nameToWrite.length();
	String zipFileEntryName		= "result_"+_STR(nameLength)+".rsrc";

    if (idToWrite.length() != 8)
    {
        return (Result::fail("MAC native, id to write for Carbon information is not 8 characters \""+idToWrite+"\""));
    }

	MemoryInputStream zipStream (BinaryData::RSRC_zip, BinaryData::RSRC_zipSize, false);
    ZipFile zipFile (zipStream);
	const ZipFile::ZipEntry *zipFileEntry = zipFile.getEntry(zipFileEntryName);

	_DBG("INSTANCE: trying to use zip file entry with name: "+zipFileEntryName);

	if (zipFileEntry)
	{
		_DBG("\tgot it");
		ScopedPointer<InputStream> is (zipFile.createStreamForEntry(*zipFileEntry));

		if (is)
		{
			MemoryBlock data;
			is->readIntoMemoryBlock (data, is->getTotalLength());

			if (data.getSize() > 0)
			{
				/* name data start 261 */
				data.removeSection (261, 3 + nameLength);

				/* id data startx 579 - after the name is removed*/
				data.removeSection (299, 8);
				data.insert (idToWrite.toUTF8().getAddress(), 8, 299);

				data.insert (nameToWrite.toUTF8().getAddress(), 3+nameLength, 261);

				if (rsrcFile.hasWriteAccess())
				{
					if (rsrcFile.replaceWithData (data.getData(), data.getSize()))
					{
						return (Result::ok());
					}
					else
					{
						return (Result::fail ("MAC native, can't replace rsrc contents with custom data: "+rsrcFile.getFullPathName()));
					}
				}
				else
				{
					Result::fail ("MAC native, can't write to bundle's rsrc file at: "+rsrcFile.getFullPathName());
				}
			}
		}
	}
	else
	{
		return (Result::fail("MAC native, can't find a resource file with name: \""+zipFileEntryName+"\""));
	}

	return (Result::fail("MAC reached end of setBundleInfoCarbon() without any usable result"));
}
#endif
