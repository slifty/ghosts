#include "cinder/app/AppCocoaTouch.h"
#include "cinder/Camera.h"
#include "cinder/gl/Texture.h"
#include "cinder/gl/gl.h"
#include "cinder/Capture.h"
#include "cinder/ImageIo.h"
#include "cinder/Rand.h"
#include "cinder/Xml.h"
#include <vector>
#include <map>
#include <list>
#include <sstream>
#include <stdio.h>
#include <math.h>
#include "cinder/Text.h"
#include "cinder/Font.h"

using namespace std;
using namespace ci;
using namespace ci::app;

int SCREEN_WIDTH = 1024;
int SCREEN_HEIGHT = 768;
int TILE_WIDTH = 256; //must be a factor of 1024 for math to work out
int TILE_HEIGHT = 2048; //must be a factor of 1024 for math to work out
int SCREEN_ROWS = ceil(SCREEN_HEIGHT / TILE_HEIGHT) + 2;
int SCREEN_COLS = ceil(SCREEN_WIDTH / TILE_WIDTH) + 2;
int LOWEST_ROW = 0;

int ISREADY = false;

//Structure that contains information on a data object in the panorama
struct Projection {
    int id;
    int offX;
    int offY;
    int height;
    int width;
    Surface imageFile;
    vector<string> textLines;
    vector<string> captionLines;
    TextLayout info;
    TextLayout caption;
    gl::Texture infoImage;
    gl::Texture captionImage;
    int copiedBy;
    bool hasImage;
};

class GhostsApp : public AppCocoaTouch {
public:
	virtual void	setup();
    void	prepareSettings( Settings *settings );
    virtual void    update();
    virtual void    rotated(Vec3f rotation);
	virtual void	draw();
    
    void reset();
    
    void	touchesBegan( TouchEvent event );
	void	touchesMoved( TouchEvent event );
	void	touchesEnded( TouchEvent event );
	
	CameraPersp		mCam;
    
    
    Surface           mainImage;
    int               liveIndex; // The upper left quad index
    vector<gl::Texture>   mLiveTextures; // The four textures being rendered
    vector<gl::Texture>   mGhostSurfaces; // All the available ghost surfaces    
    
    vector<Projection>  mProjections;
    bool                astralActivated;
    int                 whichAstral;
    float               astralShiftX;
    float               astralShiftY;
    
    int ghostWidth;
    int ghostHeight;
    int ghostRows;
    int ghostCols;
    
    int baseRow;
    int baseCol;
    
    float capturePitch;
    float captureRoll;
    float captureYaw;
    float modPitch;
    float modRoll;
    float modYaw;
    bool isCalibrate;
    
    Surface tooHighIndicator;// image that is displayed if iPad looks too high
    Surface tooLowIndicator;//  image that is displayed if iPad looks too low
    Surface scanning; //displays when scanning
    gl::Texture buttonSurface; //indicator on top of an object
    gl::Texture selectedObject;//inverted indicator on top of object (signal that it's being scanned)
    gl::Texture pause;//pause button
    gl::Texture play;//play button
    gl::Texture calibrate; //calibration button
    gl::Texture switchPanorama; //switch panorama button
    
    float actualX; //X pixel on complete panorama
    float actualY; //Y pixel on complete panorama
    float xOffset; //global X location of where top left corner is on the canvas
    float yOffset; //global Y location of where top left corner is on the canvas
    float zPitch;  //global pitch
    float oldTime; //time since an object has first been scanned (-1.0 means no object is being scanned)
    float SCAN_TIME; //amount of time object has to be in scanning box to trigger event
    float dist; //distance of scanned object from the center
    float timeTouched; //time of last touch
    
    int scannerX; //Width of scanning rectangle
    int scannerY; //Height of scanning rectangle
    int objectScanned; //current object being scanned
    int displayedObject; //object whose image is being displayed
    
    int canopyID; //ID of the current canopy being viewed
    
    bool isPaused; //if image shouldn't be updated
    bool recentlyTouched; //if the screen has been touched
    bool showScanBox; //if the scan box should be shown
    
    int top; //used to track how many objects are above the screen
    int right; //used to track how many objects are to the right of screen
    int bottom; //used to track how many objects are below the screen
    int left; //used to track how many objects are to the left of the screen
    
    int colsOnEdges; //how many columns are added to the edge besides the ones that cover the unedited panorama
    
    Vec3f rotation;
    
    bool onLoadScreen; //if on the load screen
    vector<string> tableOfCanopies; //list of the canopies available
    gl::Texture canopyIndexes; //textures of the canopy indecies
    vector<gl::Texture> numberPadNumbers; //number pad textures
    bool loadingIndexes; //if it should load the list of indecies
    
    int firstDigit;
    int secondDigit;
    bool notVaildID;
    bool shouldUpdateImage;
    
    float actuX; //the actual x position calculated by small delta moves
    float currX; //the new X gyro reading
    float prevX; //the old actual x position
    
    vector<int> canopiesOnServer; //list of ID numbers of canopies on the server 
    
    int lastCanopyID; //previous canopy
    
};

void GhostsApp::reset(){ //resets the app so it can load a new panorama
    
    lastCanopyID = canopyID;
    mLiveTextures.clear();
    mGhostSurfaces.clear();
    onLoadScreen = true;
    ISREADY = false;
    disableRotation();
    shutdown();
    setup();
}

void GhostsApp::setup()
{
    canopyID = -1; //indicates no canopy is loaded
    lastCanopyID = -1;
    canopiesOnServer.resize(0);
    
    isPaused = false; //isn't paused
    isCalibrate = false; //isn't being calibrated
    recentlyTouched = false; //hasn't been touched recently
    showScanBox = false; //doesn't show scanning box
    
    dist = pow(150.0, 2) + pow(100.0, 2);//default biggest distance a scanned object can be from center
    
    scannerX = 300;//width of scanning box
    scannerY = 200;//height of scanning box
    
    objectScanned = -1;//no object being scanned
    displayedObject = -1;//no current object being displayed
    
    oldTime = -1.0;//default time (no reference time)
    SCAN_TIME = 1.0;//time object needs to be in scanning box to display text/image
    
    //default readings to adjust image to
    capturePitch = 0;
    captureRoll = 0;
    captureYaw = 0;
    modPitch = 0;
    modRoll = 0;
    modYaw = 0;
    liveIndex = -1;
    astralActivated = false;
    baseRow = 0;
    baseCol = 0;
    
    onLoadScreen = true;
    loadingIndexes = true;
    shouldUpdateImage = true;
    
    //indicates no digits are entered
    firstDigit = -1; 
    secondDigit = -1;
    
    notVaildID = false; //looking for a valid ID
    actuX = -1.0; //indicates no precious actual X is stored
    
    gl::setMatricesWindow( getWindowWidth(), getWindowHeight() ); //sets OpenGL to use the screen bounds
}

void GhostsApp::prepareSettings( Settings *settings )
{
	settings->enableMultiTouch();
}

void GhostsApp::update() {
}

void GhostsApp::rotated( Vec3f rotation )
{
	this->rotation = rotation;
    
    //if touch is active, update image offsets
    if (isCalibrate) {
        modPitch = rotation.x - capturePitch;
        modRoll = rotation.z - captureRoll;
        modYaw = rotation.y - captureYaw;
    }
}

void GhostsApp::touchesBegan( TouchEvent event )
{    
    //Loading Screen Controls:
    if (onLoadScreen){
        
        //define x and y coordinates of the touch
        for( vector<TouchEvent::Touch>::const_iterator touchIt = event.getTouches().begin(); touchIt != event.getTouches().end(); ++touchIt ) {
            
            int x = touchIt->getPos().x;
            int y = touchIt->getPos().y;
            
            int touchedDigit = -1;
            
            //reads input from the number pad
            //The "go" and "back" button are 10 and -10 respectively
            if (y < 782 && y > 712){
                if (x < 374 && x > 304){
                    touchedDigit = 1;
                }
                else if (x < 294 && x > 224){
                    touchedDigit = 4;
                }
                else if (x < 214 && x > 144){
                    touchedDigit = 7;
                }
                else if (x < 134 && x > 63){
                    touchedDigit = -10;//back
                }
            }
            else if (y < 862 && y > 792){
                if (x < 374 && x > 304){
                    touchedDigit = 2;
                }
                else if (x < 294 && x > 224){
                    touchedDigit = 5;
                }
                else if (x < 214 && x > 144){
                    touchedDigit = 8;
                }
                else if (x < 134 && x > 63){
                    touchedDigit = 0;
                }
            }
            else if (y < 942 && y > 872){
                if (x < 374 && x > 304){
                    touchedDigit = 3;
                }
                else if (x < 294 && x > 224){
                    touchedDigit = 6;
                }
                else if (x < 214 && x > 144){
                    touchedDigit = 9;
                }
                else if (x < 134 && x > 63){
                    touchedDigit = 10;//go
                }
            }
            
            //if a number was touched, register it as the first digit, second digit, or nothing
            //if the back button was touched, delete the last inputted digit
            //if the go button was touched, check if it is a validID
            if (touchedDigit != -1 && firstDigit == -1){
                if (touchedDigit != -10){
                    firstDigit = touchedDigit;
                }
            }
            else if (touchedDigit == -10 && firstDigit != -1 && secondDigit == -1){
                firstDigit = -1;
            }
            else if (touchedDigit != -1 && firstDigit != -1 && secondDigit == -1){
                if (touchedDigit != -10 && touchedDigit != 10){
                    secondDigit = touchedDigit;
                }
            }
            else if (touchedDigit == -10 && firstDigit != -1 && secondDigit != -1){
                secondDigit = -1;
                notVaildID = false;
            }
            
            //If the number is a validID, set canopy to load to be that ID 
            else if (touchedDigit == 10 && firstDigit != -1 && secondDigit != -1){
                canopyID = 10 * firstDigit + secondDigit;
                bool isValidID = false;
                for (int o = 0; o < canopiesOnServer.size(); o++){
                    if (canopyID == canopiesOnServer[o]){
                        isValidID = true;
                    }
                }
                if (!isValidID){
                    canopyID = -1;
                    notVaildID = true;
                }
            }
        }
    }
    
    //Image Veiwing Controls:
    else{
        
        recentlyTouched = true;
        timeTouched = getElapsedSeconds();
        
        //define x and y coordinates of the touch
        for( vector<TouchEvent::Touch>::const_iterator touchIt = event.getTouches().begin(); touchIt != event.getTouches().end(); ++touchIt ) {
            
            int xTouch = touchIt->getPos().x;
            int yTouch = touchIt->getPos().y;
            
            actualX = xOffset - (768.0 - (float)xTouch); //pixel X location relative to the image of touch
            actualY = yOffset - (float)yTouch; //pixel y location relative to the image of touch
            
            //switches panorama if bottom right corner touched
            if (yTouch > 924 && xTouch < 100){
                reset();
            }
            
            //pause button (stops updating the image if paused)
            if (yTouch < 120){
                if (isPaused){
                    isPaused = false;
                }
                else{
                    isPaused = true;
                }
            }
            
            // this virtual box lets you calibrate the canopy
            if ((xTouch > 650) && (yTouch > 900)) {
                isCalibrate = true;
                capturePitch = rotation.x - modPitch;
                captureRoll = rotation.z - modRoll;
                captureYaw = rotation.y - modYaw;
            }
        }
    }
}

void GhostsApp::touchesMoved( TouchEvent event )
{
}

void GhostsApp::touchesEnded( TouchEvent event )
{
    isCalibrate = false;
}

void GhostsApp::draw()
{
    //Draw the load screen
    if (onLoadScreen){
        gl::clear(Color(0,0,0));
        if (loadingIndexes){
            
            //loads all the canopies' name and ID number
            ostringstream tableInfo;
            tableInfo.str("");
            XmlTree canopies(loadUrl("http://ghosts.slifty.com/services/getCanopyList.php"));
            list<XmlTree> listOfCanopies = canopies.getChild("canopies").getChildren();
            list<XmlTree>::iterator i;
            tableOfCanopies.resize(listOfCanopies.size() + 2);
            
            
            //creates a table of contents with the index and names of each panorama
            int tableIndex = 2;
            tableOfCanopies[0] = "List of Canopies and their index numbers:";
            tableOfCanopies[1] = "  ";
            for(i = listOfCanopies.begin(); i != listOfCanopies.end(); ++i, tableIndex ++) {
                XmlTree canopyOption = *i;
                tableInfo.str("");
                tableInfo << canopyOption.getChild("id").getValue().c_str() << ". " << canopyOption.getChild("name").getValue().c_str();
                tableOfCanopies[tableIndex] = tableInfo.str();
                
                canopiesOnServer.resize(canopiesOnServer.size() + 1);
                canopiesOnServer[canopiesOnServer.size() - 1] = atoi(canopyOption.getChild("id").getValue().c_str());
            }
            
            //creates TextLayout for the table of contents
            TextLayout canopyTableOfContents;
            canopyTableOfContents.clear(ColorA(0.0f,0.0f,0.0f,1.0));
            canopyTableOfContents.setFont(Font("Arial", 20));
            canopyTableOfContents.setColor(Color(10.0f,10.0f,10.0f));
            
            //fills table of contents
            for (int u = 0; u < tableOfCanopies.size(); u++){
                canopyTableOfContents.addLine(tableOfCanopies[u]);
            }
            
            //creates an image fot the table of contents
            canopyIndexes = gl::Texture(canopyTableOfContents.render(true, false));
            
            //Creates a table of each number button
            //also creates a list of rendered images for each number buttom
            vector<TextLayout> numberPad;
            numberPad.resize(12);
            ostringstream temp; //the number to add to the table of numbers
            numberPadNumbers.resize(12);
            for (int r = 0; r < numberPad.size(); r++){
                numberPad[r].clear(ColorA(1.0f,1.0f,1.0f,1.0));
                numberPad[r].setFont(Font("Arial", 50));
                numberPad[r].setColor(Color(0.0f,0.0f,0.0f));
                if(r < 9){
                    temp.str("");
                    temp << r + 1;
                    numberPad[r].addLine(temp.str());
                    numberPadNumbers[r] = gl::Texture(numberPad[r].render(true, false));
                }
                else if (r == 9){
                    numberPad[r].setFont(Font("Arial", 25));
                    numberPad[r].addLine("Back");
                    numberPadNumbers[r] = gl::Texture(numberPad[r].render(true, false));
                }
                else if (r == 10){
                    numberPad[r].addLine("0");
                    numberPadNumbers[r] = gl::Texture(numberPad[r].render(true, false));
                }
                else if (r == 11){
                    numberPad[r].setFont(Font("Arial", 25));
                    numberPad[r].addLine("Go");
                    numberPadNumbers[r] = gl::Texture(numberPad[r].render(true, false));
                }
            }

            loadingIndexes = false; //done loading the number pad and table of contents
        }
        
        //Draws the Title and directions
        glPushMatrix();
        glTranslatef(400, 50, 0);
        glRotatef(90, 0, 0, 1);
        gl::drawStringCentered("Ghosts Of The Past", Vec2f(525,-350),ColorA(1, 1, 1, 1), Font("Arial", 50));
        gl::drawStringCentered("Input an index number below to display a panorama.", Vec2f(525,-250),ColorA(1, 1, 1, 1), Font("Arial", 20));
        gl::draw(canopyIndexes);
        glPopMatrix();
        
        //draws the number pad
        glPushMatrix();
        glRotatef(90, 0, 0, 1);
        
        //boxes of 70x70
        gl::drawSolidRect(Rectf(712, -304, 782, -374));//1
        gl::draw(numberPadNumbers[0],Vec2f(732, -369));
        
        gl::drawSolidRect(Rectf(792, -304, 862, -374));//2
        gl::draw(numberPadNumbers[1],Vec2f(812, -369));
        
        gl::drawSolidRect(Rectf(872, -304, 942, -374));//3
        gl::draw(numberPadNumbers[2],Vec2f(892, -369));
        
        gl::drawSolidRect(Rectf(712, -224, 782, -294));//4
        gl::draw(numberPadNumbers[3],Vec2f(732, -284));
        
        gl::drawSolidRect(Rectf(792, -224, 862, -294));//5
        gl::draw(numberPadNumbers[4],Vec2f(812, -284));
        
        gl::drawSolidRect(Rectf(872, -224, 942, -294));//6
        gl::draw(numberPadNumbers[5],Vec2f(892, -284));
        
        gl::drawSolidRect(Rectf(712, -144, 782, -214));//7
        gl::draw(numberPadNumbers[6],Vec2f(732, -204));
        
        gl::drawSolidRect(Rectf(792, -144, 862, -214));//8
        gl::draw(numberPadNumbers[7],Vec2f(812, -204));
        
        gl::drawSolidRect(Rectf(872, -144, 942, -214));//9
        gl::draw(numberPadNumbers[8],Vec2f(892, -204));
        
        gl::drawSolidRect(Rectf(712, -63, 782, -134));//back
        gl::draw(numberPadNumbers[9],Vec2f(719, -112));
        
        gl::drawSolidRect(Rectf(792, -63, 862, -134));//0
        gl::draw(numberPadNumbers[10],Vec2f(812, -124));
        
        gl::drawSolidRect(Rectf(872, -63, 942, -134));//go
        gl::draw(numberPadNumbers[11],Vec2f(888, -114));
        
        ostringstream inputtedIndex; //the digits inputted by the user
        inputtedIndex.str("");
        
        //Draw the numbers inputed by the user on the screen
        gl::drawStringCentered("Inputed Digits:", Vec2f(749, -474), ColorA(1,1,1,1), Font("Arial", 30));
        if (firstDigit != -1){
            inputtedIndex << firstDigit;
            gl::drawStringCentered(inputtedIndex.str(), Vec2f(849, -474), ColorA(1,1,1,1), Font("Arial", 40));
        }
        if (secondDigit != -1){
            inputtedIndex.str("");
            inputtedIndex << secondDigit;
            gl::drawStringCentered(inputtedIndex.str(), Vec2f(874, -474), ColorA(1,1,1,1), Font("Arial", 40));
        }
        //if not a valid ID, tell the user so
        if (notVaildID){
            gl::drawStringCentered("*not a vaild index!", Vec2f(950, -464));
        }
        
        glPopMatrix();
        
        //if a canopy has been selected, clear the screen, tell the user the panorama is loading, and load the selected panorama
        if(canopyID != -1){
            onLoadScreen = false;
            gl::clear(Color(0,0,0));
            glPushMatrix();
            glTranslatef(434,300,0);
            glRotatef(90,0,0,1);
            
            TextLayout loadingPanorama;
            loadingPanorama.clear(ColorA(0.0f,0.0f,0.0f,1.0));
            loadingPanorama.setFont(Font("Arial", 50));
            loadingPanorama.setColor(Color(10.0f,10.0f,10.0f));
            loadingPanorama.addLine("Loading Panorama...");
            gl::Texture loadingPano = gl::Texture(loadingPanorama.render(true, false));
            gl::draw(loadingPano);
            glPopMatrix();
        }
    }
    
    //If not on the load screen anymore
    else{
        if(!ISREADY) {
            
            // loading image from ghosts
            ostringstream oss;
            oss << "http://ghosts.slifty.com/services/getCanopyInformation.php?c=" << canopyID;
            
            //Loads the projections
            XmlTree doc( loadUrl( oss.str() ) );
            console() << oss.str() << std::endl;
            XmlTree canopy = doc.getChild("canopy");
            XmlTree projections = canopy.getChild("projections");
            
            //Loads the panorama's dimensions
            ghostHeight = atoi(canopy.getChild("height").getValue().c_str());
            ghostWidth = atoi(canopy.getChild("width").getValue().c_str());
            
            //Loads in projection data 
            list<XmlTree> L = projections.getChildren();
            list<XmlTree>::iterator i;
            mProjections.resize(L.size());
            
            //creates all projection structures
            int cursor = 0;
            for(i = L.begin(); i != L.end(); ++i, ++cursor) {
                XmlTree projectionXML = *i;
                
                Projection proj;
                
                //loads its location on the canopy and its ID number
                proj.id = atoi(projectionXML.getChild("id").getValue().c_str());
                proj.offX = atoi(projectionXML.getChild("offX").getValue().c_str())+ 1024;
                proj.offY = atoi(projectionXML.getChild("offY").getValue().c_str());
                proj.height = atoi(projectionXML.getChild("height").getValue().c_str());
                proj.width = atoi(projectionXML.getChild("width").getValue().c_str());
                
                //prints out projection's URL location
                oss.str("");
                oss << "http://ghosts.slifty.com/services/getPortalImage.php?p=" << proj.id;
                console() << oss.str() << std::endl;
                
                //checks if the the projection has an image and loads it if it does
                if (atoi(projectionXML.getChild("width").getValue().c_str()) != 0){
                    proj.imageFile = Surface(loadImage( loadUrl( oss.str() ) ));
                    proj.hasImage = true;
                }
                else{
                    proj.hasImage = false;
                }
                
                //loads text gotten from server
                string text = projectionXML.getChild("description").getValue(); 
                
                //loads caption gotten from server
                string caption = projectionXML.getChild("caption").getValue();
                if (!proj.hasImage){
                    caption = " ";
                }
                
                //Lines used to store text/caption characters per TextLayout
                string line = "";
                string captionLine = "";
                int place = 0;
                int place2 = 0;
                
                int tracker = 0;//keeps track of the current amount of characters in a line
                int captionTracker = 0;//keeps track of current amount of characters in the caption
                int resizer = 1;
                
                proj.textLines.resize(resizer);
                proj.captionLines.resize(resizer);
                
                //Adds text to the structure
                for (int q = 0; q < text.size() || q < caption.size(); q++){
                    
                    //Cuts down information text
                    if (q < text.size()){
                        line.append(text,q,1);//adds a character to the current line
                        
                        if (tracker < 40){//if the line is shorter than 40, keep adding to the line
                            if (q == text.size() - 1){//if the end of the text has been reached, add the line to be displayed
                                proj.textLines[place] = line;
                                place++;
                            }
                            tracker++;
                        }
                        else{
                            if (text.compare(q,1, " ") == 0){//if after a set of 40 characters there is a space, go to the next line
                                resizer++;
                                proj.textLines.resize(resizer);
                                proj.textLines[place] = line;
                                place++;
                                line = "";
                                tracker = 0;
                            }
                            else if (tracker == 45){//if the last word goes over the limit for a line, it is broken up with a "-"
                                resizer++;
                                proj.textLines.resize(resizer);
                                line.append("-");
                                proj.textLines[place] = line;
                                place++;
                                line = "";
                                tracker = 0;
                            }
                            else{//if still on a word but less than 4 characters, keep adding characters
                                tracker++;
                            }
                        }
                    }
                    
                    //Cuts down caption text
                    if (q < caption.size()){
                        captionLine.append(caption,q,1);//adds a character to the current line
                        
                        if (captionTracker < 40){//if the line is shorter than 40, keep adding to the line
                            if (q == caption.size() - 1){//if the end of the text has been reached, add the line to be displayed
                                proj.captionLines[place2] = captionLine;
                                place2++;
                            }
                            captionTracker++;
                        }
                        else{
                            if (caption.compare(q,1, " ") == 0){//if after a set of 40 characters there is a space, go to the next line
                                resizer++;
                                proj.captionLines.resize(resizer);
                                proj.captionLines[place2] = captionLine;
                                place2++;
                                captionLine = "";
                                captionTracker = 0;
                            }
                            else if (captionTracker == 45){//if the last word goes over the limit for a line, it is broken up with a "-"
                                proj.captionLines.resize(resizer);
                                captionLine.append("-");
                                proj.captionLines[place2] = captionLine;
                                place2++;
                                captionLine = "";
                                captionTracker = 0;
                            }
                            else{//if still on a word but less than 4 characters, keep adding characters
                                captionTracker++;
                            }
                        }
                    }
                }
                
                //adds the text to be displayed on the screen
                gl::enableAlphaBlending();//enables transparency
                float clearAlpha = 0.5f;// set transparency value
                TextLayout textLayout;//information text
                TextLayout captionText; // caption text
                
                captionText.clear(ColorA(0.0f,0.0f,0.0f,clearAlpha));//caption backgroud color
                captionText.setFont(Font("Arial", 14));//caption font
                captionText.setColor(Color(1.0f,1.0f,1.0f));//caption text color
                
                textLayout.clear(ColorA(0.0f,0.0f,0.0f,clearAlpha));//backgroud color
                textLayout.setFont(Font("Arial", 18));//font
                textLayout.setColor(Color(1.0f,1.0f,1.0f));//text color
                
                //add text lines for the object
                for (int t = 0; t < proj.textLines.size(); t++){
                    textLayout.addLine(proj.textLines[t]);
                }
                for (int t = 0; t < proj.captionLines.size(); t++){
                    captionText.addLine(proj.captionLines[t]);
                }
                proj.info = textLayout;
                proj.caption = captionText;
                
                proj.infoImage = gl::Texture(proj.info.render(true, false));
                proj.captionImage = gl::Texture(proj.caption.render(true, false));
                
                proj.copiedBy = -1;
                
                //assigns structure
                if (proj.offX >= ghostWidth){
                    
                    proj.copiedBy = cursor + 1;
                    mProjections[cursor] = proj;
                    
                    proj.offX -= (ghostWidth);
                    proj.copiedBy = -2;
                    cursor++;
                    mProjections.resize(mProjections.size() + 1);
                    mProjections[cursor] = proj;
                }
                else{
                    mProjections[cursor] = proj;
                }
                
            }
            
            // Load canopy data
            
            console() << "gh " << ghostHeight << std::endl;
            console() << "gw " << ghostWidth << std::endl;
            
            ghostRows = ceil(ghostHeight / (float)TILE_HEIGHT);
            ghostCols = ceil(ghostWidth / (float)TILE_WIDTH);
            mLiveTextures.resize(SCREEN_ROWS * SCREEN_COLS);
            mGhostSurfaces.resize(ghostRows * (ghostCols + (1024 / TILE_WIDTH)));
            
            int ghostIndex = ghostRows * (1024 / TILE_WIDTH);
            int startIndex = 0;
            
            oss.str("");
            oss << "http://ghosts.slifty.com/services/getCanopyImage.php?c=" << canopyID << "&h=" << ghostHeight << "&w=" << ghostWidth << "&x=" << 0 << "&y=" << 0;
            console() << oss.str() << std::endl;
            
            Surface panorama = Surface( loadImage( loadUrl( oss.str() ) ));
            
            for(int x = 0; x < ghostCols ; ++x) {
                for(int y = 0; y < ghostRows ; ++y) {
                    
                    int gX = TILE_WIDTH * x;
                    int gY = TILE_HEIGHT * y;
                    int gH = min(TILE_HEIGHT, ghostHeight - TILE_HEIGHT * y);
                    int gW = min(TILE_WIDTH, ghostWidth - TILE_WIDTH * x);
                    
                    mGhostSurfaces[ghostIndex] = gl::Texture(Surface(panorama.clone(Area(Vec2f(gX, gY), Vec2f(gX + gW, gY + gH)))));
                    ghostIndex++;
                    
                    //adds end to beginning of image
                    if (x == ghostCols - 1){
                        gW = TILE_WIDTH;
                        for (int u = (1024 / TILE_WIDTH); u > 0; u--){
                            gX = ghostWidth - TILE_WIDTH * u;
                            mGhostSurfaces[startIndex] = gl::Texture(Surface(panorama.clone(Area(Vec2f(gX, gY), Vec2f(gX + gW, gY + gH)))));
                            startIndex++;
                        }
                    }
                }
            }
            
            console() << "col " << ghostCols << std::endl;
            console() << "row " << ghostRows << std::endl;
            
            ghostWidth += 1024;
            ghostCols += (1024 / TILE_WIDTH);
            
            
            // loading buttons and surfaces
            buttonSurface = gl::Texture(Surface( loadImage( loadResource( "Data.jpg"))));
            selectedObject = gl::Texture(Surface(loadImage(loadResource("DataInverted.jpg"))));
            pause = gl::Texture(Surface(loadImage(loadResource("PauseButton.jpg"))));
            play = gl::Texture(Surface(loadImage(loadResource("PlayButton.jpg"))));
            calibrate = gl::Texture(Surface(loadImage(loadResource("Calibrate.jpg"))));
            switchPanorama = gl::Texture(Surface(loadImage(loadResource("SwitchPanorama.jpg"))));
            
            ISREADY = true;
            enableRotation();
        } 
        
        // default is canopy
        else {

            gl::clear( Color( 0.0f, 0.0f, 0.0f ) );
            
            float pixelXOffset; //right tilt (- towards) (+ away)
            float pixelYOffset; // top tilt (+ towards) (- away)
            float pitch; // center spin (+ clockwise) (- counter clockwise)
            
            
            // calculating change in gyro for Y direction, roll
            pixelYOffset = -((rotation.z - modRoll + 3.1415/2) / (2 * 3.1415)) * ghostWidth - 1200.0f;
            
            //updates the actual X offset based on the difference between the past and current gyro reading
            if(actuX == -1.0){
                actuX = rotation.y;
                currX = rotation.y;
                prevX = rotation.y;
            }
            else{
                currX = rotation.y;
                
                if (prevX == currX){
                    actuX = actuX;
                }
                else if (prevX > 0 && currX < 0){
                    if(abs(prevX) < 0.5 && abs(currX) < 0.5){
                        actuX -= abs(prevX + currX);
                    }
                    else{
                        actuX += abs(prevX + currX);
                    }
                }
                else if (prevX < 0 && currX < 0){
                    if (prevX > currX){
                        actuX -= abs(prevX - currX);
                    }
                    else if (prevX < currX){
                        actuX += abs(prevX - currX);
                    }
                }
                else if (prevX > 0 && currX > 0){
                    if (prevX > currX){
                        actuX -= abs(prevX - currX);
                    }
                    else if (prevX < currX){
                        actuX += abs(prevX - currX);
                    }
                }
                else if (prevX < 0 && currX > 0){
                    if(abs(prevX) < 0.5 && abs(currX) < 0.5){
                        actuX += abs(prevX + currX);
                    }
                    else{
                        actuX -= abs(prevX + currX);
                    }
                }
                
                prevX = currX;
            }
            
            // calculating change in gyro for X direction (ipad is being held sideways), yaw            
            pixelXOffset = ((actuX - modYaw)  / (2 * 3.1415)) * ghostWidth;
            
            //Transfers view to other end of image if reaches the end
            
            while (pixelXOffset >= 0) { 
                pixelXOffset -= (ghostWidth - SCREEN_WIDTH);
                
            }
            while(pixelXOffset < -(ghostWidth -  SCREEN_WIDTH)){
                pixelXOffset += (ghostWidth - SCREEN_WIDTH);
            }
            
            // calculating change in gyro for Z direction, pitch
            pitch = (rotation.x - modPitch) / (2 * 3.1415) * 360.0f;
            
            if (isPaused){
                pixelXOffset = xOffset;
                pixelYOffset = yOffset;
                pitch = zPitch;
            }
            //Sets to a global variable
            xOffset = pixelXOffset;
            yOffset = pixelYOffset;
            zPitch = pitch;
            
            // Figure out the base row / column to view
            int gR = ghostRows - 1 - (((int)floor(pixelYOffset / TILE_HEIGHT) + ghostRows));
            
            LOWEST_ROW = floor(ghostHeight / TILE_HEIGHT);//Keeps iPad on the image
            
            if (gR < 0){
                gR = 0;
            }
            if (gR > LOWEST_ROW){
                gR = LOWEST_ROW;
            }                                           
            
            int gC = ghostCols - 1 - (((int)floor(pixelXOffset / TILE_WIDTH) + ghostCols));
            
            int usedRows = min(SCREEN_ROWS, ghostRows);
            int usedCols = min(SCREEN_COLS, ghostCols);
            
            int index = gC * ghostRows + gR;
            if(index != liveIndex) {
                
                int b = 0;
                int t = usedRows;
                int l = 0;
                int r = usedCols;
                
                if(liveIndex != -1) {
                    // Calculate the amount shifted (on the entire canopy)
                    int rShift = index % ghostRows - liveIndex % ghostRows;
                    int cShift = (index - index % ghostRows)/ghostRows - (liveIndex - liveIndex % ghostRows)/ghostRows;
                    
                    // This covers an edge case where the "lip" is passed over
                    if(abs(rShift) < usedRows && abs(cShift) < usedCols) {
                        
                        // Convert that to an amount shifted on the texture matrix
                        rShift = rShift % usedRows;
                        cShift = cShift % usedCols;
                        
                        if(cShift < 0) { // pan left (shift right)
                            r = -cShift;
                            for(int c = usedCols - 1; c >= -cShift; --c)
                                for(int r = 0; r < usedRows ; ++r){
                                    mLiveTextures[c * usedRows + r] = mLiveTextures[ (c + cShift) * usedRows + r];
                                }
                        }
                        
                        if(cShift > 0) { // pan right (shift left)
                            l = usedCols - cShift;
                            for(int c = 0; c < usedCols - cShift ; ++c)
                                for(int r = 0; r < usedRows ; ++r){
                                    mLiveTextures[c * usedRows + r] = mLiveTextures[ (c + cShift) * usedRows + r];
                                }
                        }
                    }    
                }
                
                // Fill outdated data
                for(int c = l; c < r ; ++c) {
                    for(int r = b; r < t ; ++r) {
                        int i = ((gC + c) % ghostCols) * ghostRows + ((gR + r) % ghostRows);
                        mLiveTextures[ c * usedRows + r ] = mGhostSurfaces[i];
                    }
                }
                
                liveIndex = index;
            }
            
            glPushMatrix();
            
            int relXOffset = (int) pixelXOffset % TILE_WIDTH;
            int relYOffset = (int) pixelYOffset % TILE_HEIGHT - SCREEN_HEIGHT;
            
            // move the picture in accordance to gyro readings
            glRotatef(90 - pitch, 0.0, 0.0, 1.0);
            glTranslatef(relXOffset, relYOffset, 0.0f );
            
            for(int c = 0; c < usedCols ; ++c) {                                                                          
                for(int r = 0; r < usedRows ; ++r) {
                    int gH = min(TILE_HEIGHT, ghostHeight - TILE_HEIGHT * ((gR + r)));
                    int gW = min(TILE_WIDTH, ghostWidth - TILE_WIDTH * ((gC + c)));
                    
                    glPushMatrix();
                    glTranslatef(c * TILE_WIDTH, r * TILE_HEIGHT, 0.0f );
                    gl::draw(mLiveTextures[ c * usedRows + r ], Rectf(0.0f, 0.0f, (float)gW, (float)gH));
                    glPopMatrix();
                }
            }
            
            // drawing notices on objects and displays text/picture if an object has been scanned
            int size = mProjections.size();
            for (int i = 0; i < size; ++i) {
                
                //only displays objects that at on the screen (not off)
                if(-mProjections[i].offY < pixelYOffset && -mProjections[i].offY > (pixelYOffset - 768.0)){
                    if (-mProjections[i].offX < pixelXOffset && -mProjections[i].offX > (pixelXOffset - 1024.0)){
                        
                        if(displayedObject != i){
                            if (objectScanned == i){
                                glPushMatrix();
                                glTranslatef(mProjections[i].offX + pixelXOffset - relXOffset, mProjections[i].offY + pixelYOffset - relYOffset - SCREEN_HEIGHT, 0.0f);
                                gl::draw(selectedObject);//changes indicators icon if being scanned (pre-display)
                                glPopMatrix();
                            }
                            else{
                                //doesnt display indicator if object is being displayed
                                glPushMatrix();
                                glTranslatef(mProjections[i].offX + pixelXOffset - relXOffset, mProjections[i].offY + pixelYOffset - relYOffset - SCREEN_HEIGHT, 0.0f);
                                gl::draw(gl::Texture(buttonSurface));
                                glPopMatrix();
                            }
                        }
                        astralShiftX = pixelXOffset;
                        astralShiftY = pixelYOffset - SCREEN_HEIGHT;
                    }
                }
                
                if (abs(mProjections[i].offX + pixelXOffset) < ghostWidth/3){
                    
                    console() << pixelXOffset << endl;
                    console() << mProjections[i].offX << endl;
                    
                    if (pixelXOffset > -mProjections[i].offX && pixelXOffset - SCREEN_WIDTH < -mProjections[i].offX){
                        if (-mProjections[i].offY > pixelYOffset){
                            top++;
                        }
                        else if ((-mProjections[i].offY) < pixelYOffset - 768.0){
                            bottom++;
                        }
                    }
                    
                    if (-mProjections[i].offX > pixelXOffset){
                        left++;
                    }
                    
                    else if (-mProjections[i].offX < pixelXOffset - 1024){
                        right++;
                    }
                    
                }
            }
            
            glPopMatrix();
            
            float actualCenterX = -1 * (pixelXOffset - (SCREEN_WIDTH / 2)); //determines what the center pixel on screen is in the canvas
            float actualCenterY = -1 * (pixelYOffset - (SCREEN_HEIGHT / 2));//
            
            float objectX;//currently scanned object's X coordinate
            float objectY;//currently scanned object's Y coordinate
            
            //creates a scannerX by scannerY rectangle scanning bound at the center of the screen
            float xHalfRange = scannerX / 2; 
            float yHalfRange = scannerY / 2; 
            
            for (int object = 0; object < size; object++){
                
                if(-mProjections[object].offY < pixelYOffset && -mProjections[object].offY > (pixelYOffset - 768.0)){
                    if (-mProjections[object].offX < pixelXOffset && -mProjections[object].offX > (pixelXOffset - 1024.0)){
                        
                        if (objectScanned != -1){    //if something is being scanned, set the coordinates to that object
                            objectX = mProjections[objectScanned].offX;
                            objectY = mProjections[objectScanned].offY;
                        }
                        else{
                            objectX = mProjections[object].offX; //if nothing is being scanned, set the coordinates to the current object
                            objectY = mProjections[object].offY;
                        }
                        
                        //find the bounds the center has to be in the scan the current object
                        float xMax = (objectX + xHalfRange) + 15.0;
                        float xMin = (objectX - xHalfRange) + 15.0;
                        float yMax = (objectY + yHalfRange) + 30.0;
                        float yMin = (objectY - yHalfRange) + 30.0;
                        
                        //checks to see if the object's x and y are in the rectange scanning bounds
                        if (actualCenterX <= xMax && actualCenterX >= xMin){   
                            if (actualCenterY <= yMax && actualCenterY >= yMin){
                                
                                if (objectScanned == -1){ //checks to see if an object is currently being scanned (-1 means no)
                                    objectScanned = object;
                                }
                                else{
                                    float objectDist = pow((actualCenterX - mProjections[object].offX),2) + pow((actualCenterY - mProjections[object].offY),2);//selects object closet to center while scanning                       
                                    dist = pow((actualCenterX - objectX),2) + pow((actualCenterY - objectY),2);
                                    
                                    if(objectDist < dist){
                                        dist = objectDist;
                                        objectScanned = object;
                                    }
                                }
                                
                                //starts timer if no object is currently being scanned
                                if (oldTime == -1.0){ 
                                    oldTime = getElapsedSeconds();
                                }
                                
                                else{
                                    //displays event after SCAN_TIME seconds
                                    if (SCAN_TIME < (getElapsedSeconds() - oldTime)){ 
                                        displayedObject = objectScanned;
                                        showScanBox = false;
                                    }
                                    //displays that an object is being scanned if less than SCAN_TIME seconds
                                    else{
                                        glPushMatrix();
                                        glRotatef(90.0, 0.0, 0.0, 1.0);
                                        //glTranslatef(780.0, -100.0, 0.0);
                                        gl::drawString("Scanning...", Vec2f(750.0f,-90.0f),ColorA(1,1,1,1.0), Font("Arial", 55));
                                        glPopMatrix();
                                        showScanBox = true;
                                    }
                                }
                            }
                            else{
                                //resets time and object being scanned if object leaves scanning box
                                oldTime = -1.0; 
                                objectScanned = -1;
                                displayedObject = -1;
                                showScanBox = false;
                            }
                            
                        }
                        else{
                            //resets time and object being scanned if object leaves scanning box
                            oldTime = -1.0; 
                            objectScanned = -1;
                            displayedObject = -1;
                            showScanBox = false;
                        }
                    }
                }
            }
            
            if (recentlyTouched){ //displays screen buttons when screen is touched
                if (getElapsedSeconds() - timeTouched < 1.0 || isPaused){
                    
                    if (isPaused){
                        gl::draw(play, Rectf(0, 20.0f, 768.0f, 120.0f));
                        gl::draw(switchPanorama, Rectf(0, 924, 100, 1024));
                        showScanBox = false;
                    }
                    else{
                        gl::draw(pause, Rectf(0.0f, 20.0f, 768.0f, 120.0f));
                        gl::draw(calibrate, Rectf(668, 924, 768, 1024));
                        gl::draw(switchPanorama, Rectf(0, 924, 100, 1024));
                        showScanBox = true;
                    }
                }
                else{ //takes displayed buttons away
                    recentlyTouched = false;
                    showScanBox = false;
                }
            }
            
            if (showScanBox){ //shows the scanning box
                gl::drawLine(Vec2f(SCREEN_HEIGHT/2 - scannerY/2, SCREEN_WIDTH/2 + scannerX/2), Vec2f(SCREEN_HEIGHT/2 - scannerY/2, SCREEN_WIDTH/2 - scannerX/2));
                gl::drawLine(Vec2f(SCREEN_HEIGHT/2 - scannerY/2, SCREEN_WIDTH/2 - scannerX/2), Vec2f(SCREEN_HEIGHT/2 + scannerY/2, SCREEN_WIDTH/2 - scannerX/2));
                gl::drawLine(Vec2f(SCREEN_HEIGHT/2 + scannerY/2, SCREEN_WIDTH/2 - scannerX/2), Vec2f(SCREEN_HEIGHT/2 + scannerY/2, SCREEN_WIDTH/2 + scannerX/2));
                gl::drawLine(Vec2f(SCREEN_HEIGHT/2 + scannerY/2, SCREEN_WIDTH/2 + scannerX/2), Vec2f(SCREEN_HEIGHT/2 - scannerY/2, SCREEN_WIDTH/2 + scannerX/2));
            }
            
            //prevents duplication when going too low
            if (pixelYOffset < -ghostHeight){
                gl::clear( Color( 0.0f, 0.0f, 0.0f ) );
                glPushMatrix();
                glRotatef(90.0, 0.0, 0.0, 1.0);
                gl::drawStringCentered("Too Low: Tilt Up", Vec2f(700.0f,-150.0f),ColorA(0,0,1,1), Font("Arial", 90));
                glPopMatrix();  
            }
            //prevents duplicaiton when going too high
            if (pixelYOffset > ghostHeight){
                gl::clear( Color( 0.0f, 0.0f, 0.0f ) ); 
                glPushMatrix();
                glRotatef(90.0, 0.0, 0.0, 1.0);
                gl::drawStringCentered("Too High: Tilt Down", Vec2f(700.0f,-818.0f),ColorA(1,0,0,1), Font("Arial", 90));
                glPopMatrix();              
            }
        }
        
        if (displayedObject != -1){// if an object is being displayed, display text/pic
            
            glPushMatrix();
            glTranslatef(500, 500, 0.0f);
            glRotatef(90, 0, 0, 1);
            
            gl::enableAlphaBlending();//enables transparency
            
            //draws the text to the right of the object with an arrow pointing to it
            glPushMatrix();
            glTranslatef(150.0f, -100.0f, 0.0f);
            gl::draw(mProjections[displayedObject].infoImage);
            gl::drawVector(Vec3f(0.0f,50.0f,0.0f), Vec3f(-100.0f, 125.0f, 0.0f), 10.0f, 5.0f);
            
            //displays image gotten from server to the left of the object
            if (mProjections[displayedObject].hasImage){
                glPushMatrix();
                
                
                gl::draw(mProjections[displayedObject].captionImage, Vec2f(-(200.0 + mProjections[displayedObject].imageFile.getWidth()), mProjections[displayedObject].imageFile.getHeight()));//draws the caption text
                
                //draws the image
                gl::draw(mProjections[displayedObject].imageFile, Vec2f(-(200.0 + mProjections[displayedObject].imageFile.getWidth()), 0.0));
                
                glPopMatrix();
            }
            
            glPopMatrix();
            
            glPopMatrix();
        }
        
        //searching arrows to indicate where projections are
        if (right > 0){
            glPushMatrix();
            glTranslatef(384.0, 1000.0, 0.0);
            glRotatef(90.0, 0.0, 0.0, 1.0);
            gl::drawSolidCircle(Vec2f(0.0, 0.0f), 20.0f, 4);
            glPopMatrix();
        }
        if (left > 0){
            glPushMatrix();
            glTranslatef(384.0, 44.0, 0.0);
            glRotatef(-90.0, 0.0, 0.0, 1.0);
            gl::drawSolidCircle(Vec2f(0.0, 0.0f), 20.0f, 4);
            glPopMatrix();
        }
        if (top > 0){
            glPushMatrix();
            glTranslatef(744.0, 512, 0.0);
            gl::drawSolidCircle(Vec2f(0.0, 0.0f), 20.0f, 4);
            
            glPopMatrix();
        }
        if (bottom > 0){
            glPushMatrix();
            glTranslatef(24.0, 512.0, 0.0);
            glRotatef(180.0, 0.0, 0.0, 1.0);
            gl::drawSolidCircle(Vec2f(0.0, 0.0f), 20.0f, 4);
            glPopMatrix();
        }
        
        //resets count
        top = 0;
        bottom = 0;
        right = 0;
        left = 0;
    }
}


CINDER_APP_COCOA_TOUCH( GhostsApp, RendererGl )
