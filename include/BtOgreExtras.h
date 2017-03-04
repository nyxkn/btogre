/*
 * =====================================================================================
 *
 *       Filename:  BtOgreExtras.h
 *
 *    Description:  Contains the Ogre Mesh to Bullet Shape converters.
 *
 *        Version:  1.0
 *        Created:  27/12/2008 01:45:56 PM
 *
 *         Author:  Nikhilesh (nikki)
 *
 * =====================================================================================
 */

#ifndef _BtOgreShapes_H_
#define _BtOgreShapes_H_

#include "btBulletDynamicsCommon.h"
#include "OgreSceneNode.h"
#include "OgreSimpleRenderable.h"
#include "OgreCamera.h"
#include "OgreHardwareBufferManager.h"
#include "OgreMaterialManager.h"
#include "OgreTechnique.h"
#include "OgrePass.h"

#include "OgreLogManager.h"

namespace BtOgre
{

typedef std::vector<Ogre::Vector3> Vector3Array;

//Converts from and to Bullet and Ogre stuff. Pretty self-explanatory.
class Convert
{
public:
	Convert() {};
	~Convert() {};

	static btQuaternion toBullet(const Ogre::Quaternion &q)
	{
		return btQuaternion(q.x, q.y, q.z, q.w);
	}
	static btVector3 toBullet(const Ogre::Vector3 &v)
	{
		return btVector3(v.x, v.y, v.z);
	}

	static Ogre::Quaternion toOgre(const btQuaternion &q)
	{
		return Ogre::Quaternion(q.w(), q.x(), q.y(), q.z());
	}
	static Ogre::Vector3 toOgre(const btVector3 &v)
	{
		return Ogre::Vector3(v.x(), v.y(), v.z());
	}
};

//From here on its debug-drawing stuff. ------------------------------------------------------------------

//Draw the lines Bullet want's you to draw
class LineDrawer
{
    Ogre::String sceneManagerName;
    ///How a line is stored
    struct line
    {
        Ogre::Vector3 start;
        Ogre::Vector3 end;
        Ogre::ColourValue vertexColor;
    };

    ///Where the created objects will be attached
    Ogre::SceneNode* attachNode;

    ///The name of the HLMS datablock to use
    Ogre::String datablockToUse;

    ///Array of lines and objects to use
    std::vector<Ogre::ManualObject*> objects;
    std::vector<line> lines;

    ///TODO define something for the contatct points

public:
    LineDrawer(Ogre::SceneNode* node, Ogre::String datablockId, Ogre::String smgrName) :
        attachNode(node),
        sceneManagerName(smgrName)
    {
    }

    ~LineDrawer()
    {
        clear();
    }

    void clear()
    {
        //There's only one object used to display the lines, but contact points and other geometrical shapes could be implemented in the same way
        for (auto obj : objects)
        {
            //Ogre::LogManager::getSingleton().logMessage(Ogre::String("size : ") + std::to_string(objects.size()));
            attachNode->detachObject(obj);
            static auto smgr = Ogre::Root::getSingleton().getSceneManager(smgrName);
            smgr->destroyMovableObject(obj, obj->getMovableType());
        }
        objects.clear();
        lines.clear();
    }

    void addLine(const Ogre::Vector3& start, const Ogre::Vector3& end, const Ogre::ColourValue& value)
    {
        lines.push_back({ start, end, value });
    }

    void checkForMaterial()
    {
        Ogre::HlmsUnlit* hlmsUnlit = (Ogre::HlmsUnlit*) Ogre::Root::getSingleton().getHlmsManager()->getHlms(Ogre::HLMS_UNLIT);
        auto datablock = hlmsUnlit->getDatablock(datablockToUse);

        if (datablock) return;
        Ogre::LogManager::getSingleton().logMessage("BtOgre's datablock not found, creating...");
        auto createdDatablock = hlmsUnlit->createDatablock(datablockToUse, datablockToUse, Ogre::HlmsMacroblock(), Ogre::HlmsBlendblock(), Ogre::HlmsParamVec(), true, Ogre::BLANKSTRING, "BtOgre");

        if (!createdDatablock)
        {
            Ogre::LogManager().logMessage("Mh. Datablock hasn't been created. Weird.");
        }
    }

    void update()
    {
        checkForMaterial();
        static auto smgr = Ogre::Root::getSingleton().getSceneManager(smgrName);
        int index = 0;

        auto manualObj = smgr->createManualObject();
        manualObj->begin(datablockToUse, Ogre::OT_LINE_LIST);
        for (const auto& l : lines)
        {
            manualObj->position(l.start);
            manualObj->colour(l.vertexColor);
            //manualObj->textureCoord(0, 0);
            manualObj->index(index++);

            manualObj->position(l.end);
            manualObj->colour(l.vertexColor);
            //manualObj->textureCoord(0, 0);
            manualObj->index(index++);
        }

        manualObj->end();
        manualObj->setCastShadows(false);
        attachNode->attachObject(manualObj);
        objects.push_back(manualObj);
    }
};

class DebugDrawer : public btIDebugDraw
{
protected:
    Ogre::SceneNode *mNode;
    btDynamicsWorld *mWorld;
    //DynamicLines *mLineDrawer;
    int mDebugOn;
    static constexpr char* unlitDatablockName{ "DebugLinesGenerated" };
    const Ogre::IdString unlitDatablockId;

    //To accommodate the diffuse color -> light value "change" of meaning of the color of a fragment in HDR pipelines, multiply all colors by this value
    float unlitDiffuseMultiplier;

    bool stepped;
    Ogre::String scene;
    LineDrawer drawer;

public:
    DebugDrawer(Ogre::SceneNode* node, btDynamicsWorld* world, Ogre::String smgrName = "MAIN_SMGR") :
        mNode(node->createChildSceneNode()),
        mWorld(world),
        mDebugOn(true),
        unlitDatablockId(unlitDatablockName),
        unlitDiffuseMultiplier(200.0f),
        stepped(false),
        scene(smgrName),
        drawer(mNode, unlitDatablockName, scene)
    {
        if (!Ogre::ResourceGroupManager::getSingleton().resourceGroupExists("BtOgre"))
            Ogre::ResourceGroupManager::getSingleton().createResourceGroup("BtOgre");
    }

    ~DebugDrawer()
    {
    }

    void drawLine(const btVector3& from, const btVector3& to, const btVector3& color) override
    {
        if (stepped)
        {
            drawer.clear();
            stepped = false;
        }

        auto ogreFrom = Convert::toOgre(from);
        auto ogreTo = Convert::toOgre(to);
        Ogre::ColourValue ogreColor(color.x(), color.y(), color.z(), 1);
        ogreColor *= unlitDiffuseMultiplier;

        //std::stringstream out;
        //out << ogreColor;
        //Ogre::LogManager::getSingleton().logMessage(out.str());

        drawer.addLine(ogreFrom, ogreTo, ogreColor);
    }

    void draw3dText(const btVector3 &location, const char *textString) override
    {
    }
    void drawContactPoint(const btVector3 &PointOnB, const btVector3 &normalOnB, btScalar distance, int lifeTime, const btVector3 &color) override
    {
    }
    void reportErrorWarning(const char *warningString) override
    {
        Ogre::LogManager::getSingleton().logMessage(warningString);
    }

    //0 for off, anything else for on.
    void setDebugMode(int isOn) override
    {
        //mDebugOn = (isOn == 0) ? false : true;
        mDebugOn = isOn;

        //if (!mDebugOn)
        //	mLineDrawer->clear();
        if (!mDebugOn)
            drawer.clear();
    }

    //0 for off, anything else for on.
    int	getDebugMode() const override
    {
        return mDebugOn;
    }

    void step()
    {
        if (mDebugOn)
        {
            mWorld->debugDrawWorld();
            drawer.update();
        }
        else
        {
            drawer.clear();
        }
        stepped = true;
    }
};

}

#endif





