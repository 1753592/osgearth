/* -*-c++-*- */
/* osgEarth - Geospatial SDK for OpenSceneGraph
* Copyright 2020 Pelican Mapping
* http://osgearth.org
*
* osgEarth is free software; you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
* IN THE SOFTWARE.
*
* You should have received a copy of the GNU Lesser General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>
*/

#include <osgEarth/ImGuiUtils>
#include <osgEarth/OsgImGuiHandler.hpp>
#include <imgui_internal.h>

#include <osgEarth/Notify>
#include <osgEarth/EarthManipulator>
#include <osgEarth/ExampleResources>
#include <osgEarth/MapNode>
#include <osgEarth/TerrainEngineNode>
#include <osgEarth/Metrics>
#include <osgEarth/Lighting>
#include <osgEarth/NodeUtils>
#include <osgEarth/PhongLightingEffect>

#include <osgEarthProcedural/BiomeLayer>
#include <osgEarthProcedural/BiomeManager>
#include <osgEarthProcedural/LifeMapLayer>

#include <osgViewer/Viewer>
#include <osgDB/ReadFile>
#include <osgDB/WriteFile>
#include <osg/Uniform>
#include <iostream>

#define LC "[osgearth_biome] "

using namespace osgEarth;
using namespace osgEarth::Util;
using namespace osgEarth::Contrib;
using namespace osgEarth::Procedural;

int
usage(const char* name)
{
    OE_NOTICE
        << "\nUsage: " << name << " file.earth" << std::endl
        << MapNodeHelper().usage() << std::endl;
    return 0;
}

struct App
{
    const Map* _map;
    MapNode* _mapNode;
    EarthManipulator* _manip;
    EventRouter* _router;
    osgViewer::View* _view;
    osg::Light* _light;
};

struct LifeMapGUI
{
    App _app;
    LifeMapLayer* lifemap;
    float _splat_blend_start;
    float _splat_blend_end;
    float _splat_blend_rgbh_mix;
    float _splat_blend_normal_mix;

    LifeMapGUI(App& app) : _app(app)
    {
        lifemap = _app._map->getLayer<LifeMapLayer>();
        OE_HARD_ASSERT(lifemap != nullptr, __func__);

        _splat_blend_start = 2500.0f;
        _splat_blend_end = 500.0f;
        _splat_blend_rgbh_mix = 0.85f;
        _splat_blend_normal_mix = 0.72f;
    }

    void draw()
    {
        ImGui::Begin("LifeMap Tweaks");

        LifeMapLayer::Options& o = lifemap->options();
        
        ImGui::Checkbox("Use landcover data", &o.useLandCover().mutable_value());
        ImGui::Checkbox("Use terrain data", &o.useTerrain().mutable_value());
        if (o.useTerrain() == true)
        {
            ImGui::SliderFloat("Terrain weight", &o.terrainWeight().mutable_value(), 0.0f, 1.0f);
        }

        ImGui::SliderFloat(
            "Slope intensity",
            &o.slopeIntensity().mutable_value(),
            1.0f, 10.0f);

        if (ImGui::Button("Apply"))
        {
            _app._mapNode->getTerrainEngine()->invalidateRegion(
                { lifemap },
                GeoExtent::INVALID);
        }

        // uniforms
        ImGui::SliderFloat("Splat blend start (m)", &_splat_blend_start, 0.0f, 5000.0f);
        _app._mapNode->getOrCreateStateSet()->addUniform(new osg::Uniform("oe_splat_blend_start", _splat_blend_start));

        ImGui::SliderFloat("Splat blend end (m)", &_splat_blend_end, 0.0f, 5000.0f);
        _app._mapNode->getOrCreateStateSet()->addUniform(new osg::Uniform("oe_splat_blend_end", _splat_blend_end));

        ImGui::SliderFloat("Splat RGBH mix", &_splat_blend_rgbh_mix, 0.0f, 1.0f);
        _app._mapNode->getOrCreateStateSet()->addUniform(new osg::Uniform("oe_splat_blend_rgbh_mix", _splat_blend_rgbh_mix));

        ImGui::SliderFloat("Splat NRML mix", &_splat_blend_normal_mix, 0.0f, 1.0f);
        _app._mapNode->getOrCreateStateSet()->addUniform(new osg::Uniform("oe_splat_blend_normal_mix", _splat_blend_normal_mix));


        ImGui::End();
    }
};

struct TextureSplattingGUI
{
    App _app;
    float _blend_start;
    float _blend_end;
    float _blend_rgbh_mix;
    float _blend_normal_mix;
    float _rugged_power;
    float _dense_power;
    float _lush_power;
    float _normal_power;
    float _ao_power;
    float _brightness;
    float _contrast;

    TextureSplattingGUI(App& app) : _app(app)
    {
        _blend_start = 2500.0f;
        _blend_end = 500.0f;
        _blend_rgbh_mix = 0.85f;
        _blend_normal_mix = 0.72f;
        _rugged_power = 1.0f;
        _dense_power = 1.0f;
        _lush_power = 1.0f;
        _normal_power = 1.0f;
        _ao_power = 1.0f;
        _brightness = 1.0f;
        _contrast = 1.0f;
    }

    void draw()
    {
        ImGui::Begin("Texture Splatting");

        // uniforms
        ImGui::SliderFloat("Level blend start (m)", &_blend_start, 0.0f, 5000.0f);
        _app._mapNode->getOrCreateStateSet()->addUniform(new osg::Uniform("oe_splat_blend_start", _blend_start));

        ImGui::SliderFloat("Level blend end (m)", &_blend_end, 0.0f, 5000.0f);
        _app._mapNode->getOrCreateStateSet()->addUniform(new osg::Uniform("oe_splat_blend_end", _blend_end));

        ImGui::SliderFloat("RGBH mix", &_blend_rgbh_mix, 0.0f, 1.0f);
        _app._mapNode->getOrCreateStateSet()->addUniform(new osg::Uniform("oe_splat_blend_rgbh_mix", _blend_rgbh_mix));

        ImGui::SliderFloat("Normal mix", &_blend_normal_mix, 0.0f, 1.0f);
        _app._mapNode->getOrCreateStateSet()->addUniform(new osg::Uniform("oe_splat_blend_normal_mix", _blend_normal_mix));

        ImGui::SliderFloat("Rugged power", &_rugged_power, 0.0f, 4.0f);
        _app._mapNode->getOrCreateStateSet()->addUniform(new osg::Uniform("rugged_power", _rugged_power));

        ImGui::SliderFloat("Dense power", &_dense_power, 0.0f, 4.0f);
        _app._mapNode->getOrCreateStateSet()->addUniform(new osg::Uniform("dense_power", _dense_power));

        ImGui::SliderFloat("Lush power", &_lush_power, 0.0f, 4.0f);
        _app._mapNode->getOrCreateStateSet()->addUniform(new osg::Uniform("lush_power", _lush_power));

        ImGui::SliderFloat("Normal power", &_normal_power, 0.0f, 4.0f);
        _app._mapNode->getOrCreateStateSet()->addUniform(new osg::Uniform("normal_power", _normal_power));

        ImGui::SliderFloat("AO power", &_ao_power, 0.0f, 4.0f);
        _app._mapNode->getOrCreateStateSet()->addUniform(new osg::Uniform("ao_power", _ao_power));

        ImGui::SliderFloat("Global brightness", &_brightness, 0.0f, 4.0f);
        _app._mapNode->getOrCreateStateSet()->addUniform(new osg::Uniform("brightness", _brightness));

        ImGui::SliderFloat("Global contrast", &_contrast, 0.0f, 4.0f);
        _app._mapNode->getOrCreateStateSet()->addUniform(new osg::Uniform("contrast", _contrast));

        ImGui::End();
    }
};

struct BiomeGUI
{
    App _app;
    BiomeGUI(App& app) : _app(app)
    {
        BiomeLayer* biolayer = _app._map->getLayer<BiomeLayer>();
        OE_HARD_ASSERT(biolayer != nullptr, __func__);
    }

    void draw()
    {
        ImGui::Begin("Biomes", NULL, ImGuiWindowFlags_MenuBar);

        if (ImGui::CollapsingHeader("Active Biomes", ImGuiTreeNodeFlags_DefaultOpen))
        {
            auto biocat = _app._map->getLayer<BiomeLayer>()->getBiomeCatalog();
            auto& bioman = _app._map->getLayer<BiomeLayer>()->getBiomeManager();
            auto biomes = bioman.getActiveBiomes();
            for (auto biome : biomes)
            {
                if (ImGui::TreeNode(biome->name()->c_str()))
                {
                    for (auto cat : biome->modelCategories())
                    {
                        if (ImGui::TreeNode(cat.name()->c_str()))
                        {
                            for (auto& member : cat.members())
                            {
                                if (ImGui::TreeNode(member.asset->name()->c_str()))
                                {
                                    drawModelAsset(member.asset);
                                    ImGui::TreePop();
                                }
                            }
                            ImGui::TreePop();
                        }
                    }
                    ImGui::TreePop();
                }
            }
        }

        if (ImGui::CollapsingHeader("Resident Assets", ImGuiTreeNodeFlags_DefaultOpen))
        {
            auto biocat = _app._map->getLayer<BiomeLayer>()->getBiomeCatalog();
            auto& bioman = _app._map->getLayer<BiomeLayer>()->getBiomeManager();

            auto assets = bioman.getResidentAssets();
            for (auto& entry : assets)
            {
                const ModelAsset* asset = entry.first;
                const auto& data = entry.second;

                if (ImGui::TreeNode(asset->name()->c_str()))
                {
                    drawModelAsset(asset);
                    ImGui::TreePop();
                }
            }
        }

        ImGui::End();
    }

    void drawModelAsset(const ModelAsset* asset) //, const ModelAssetData::Pointer& data)
    {
        if (asset->modelURI().isSet())
            ImGui::Text("Model: %s", asset->modelURI()->base().c_str());
        if (asset->sideBillboardURI().isSet())
            ImGui::Text("Side BB: %s", asset->sideBillboardURI()->base().c_str());
        if (asset->topBillboardURI().isSet())
            ImGui::Text("Top BB: %s", asset->topBillboardURI()->base().c_str());
    }
};

class MainGUI : public OsgImGuiHandler
{
public:
    MainGUI(App& app) : 
        _app(app), 
        _lifemap(app),
        _biomes(app),
        _splatting(app)
    {
    }

protected:
    void drawUi(osg::RenderInfo& ri) override
    {
        _layers.draw(ri, _app._mapNode, _app._view->getCamera(), _app._manip);
        _lifemap.draw();
        _biomes.draw();
        _splatting.draw();
    }

    App& _app;
    LifeMapGUI _lifemap;
    BiomeGUI _biomes;
    TextureSplattingGUI _splatting;
    ImGuiUtil::LayersGUI _layers;
};

osg::Vec4
worldToVec4(const osg::Vec3d& ecef)
{
    osg::Vec4 result(0.0f, 0.0f, 0.0f, 1.0f);
    osg::Vec3d d = ecef;
    while (d.length() > 1e6)
    {
        d *= 0.1;
        result.w() *= 0.1;
    }
    return osg::Vec4(d.x(), d.y(), d.z(), result.w());
}

// For testing the splatting normal maps
void
setupMouseLight(App& app)
{
    SkyNode* sky = osgEarth::findTopMostNodeOfType<SkyNode>(app._view->getSceneData());
    if (!sky)
    {
        PhongLightingEffect* phong = new PhongLightingEffect();
        phong->attach(app._view->getSceneData()->getOrCreateStateSet());
    }

    app._light = new osg::Light(sky ? 1 : 0);
    app._light->setAmbient(osg::Vec4(0.2, 0.2, 0.2, 1));
    app._light->setDiffuse(osg::Vec4(1, 1, 1, 1));
    osg::LightSource* ls = new osg::LightSource();
    ls->setLight(app._light);
    app._mapNode->addChild(ls);
    GenerateGL3LightingUniforms gen;
    ls->accept(gen);
    app._light = ls->getLight();

    app._router->onMove([&](osg::View* view, float x, float y) {
        osg::Vec3d world;
        if (app._mapNode->getTerrain()->getWorldCoordsUnderMouse(view, x, y, world)) {
            osg::Vec3d n = world;
            n.normalize();
            world += n * 1.0;
            app._light->setPosition(worldToVec4(world));
        }
    });
}

int
encodeTexture(osg::ArgumentParser& args)
{
    std::string infile;
    if (args.read("--encode-texture", infile))
    {
        std::size_t pos = infile.find("_Color.", 0);
        if (pos >= 0)
        {
            std::string base = infile.substr(0, pos);
            std::string ext = osgDB::getFileExtension(infile);
            osg::ref_ptr<osg::Image> image;
            image = osgDB::readRefImageFile(base + ".oe_splat_rgbh");
            if (image.valid())
                osgDB::writeImageFile(*image.get(), base + ".oe_splat_rgbh");
            image = osgDB::readRefImageFile(base + ".oe_splat_nnra");
            if (image.valid())
                osgDB::writeImageFile(*image.get(), base + ".oe_splat_nnra");
        }
    }
    return 0;
}

int
main(int argc, char** argv)
{
    osgEarth::initialize();

    osg::ArgumentParser arguments(&argc, argv);
    if (arguments.read("--help"))
        return usage(argv[0]);

    if (arguments.find("--encode-texture") >= 0)
        return encodeTexture(arguments);

    osgViewer::Viewer viewer(arguments);

    viewer.setLightingMode(viewer.NO_LIGHT);

    // load an earth file, and support all or our example command-line options
    // and earth file <external> tags
    osg::Node* node = MapNodeHelper().load(arguments, &viewer);
    if (node)
    {
        App app;

        app._mapNode = MapNode::get(node);
        if (!app._mapNode)
            return usage("No map node");

        app._map = app._mapNode->getMap();
        app._view = &viewer;
        app._manip = new EarthManipulator(arguments);
        app._router = new EventRouter();
        viewer.addEventHandler(app._router);
        viewer.setCameraManipulator(app._manip);
        viewer.setRealizeOperation(new MainGUI::RealizeOperation());
        viewer.realize();
        viewer.getEventHandlers().push_front(new MainGUI(app));
        viewer.setSceneData(node);
        //setupMouseLight(app);

        return viewer.run();
    }
    else
    {
        return usage(argv[0]);
    }
}
