#include "plugin.hpp"

Plugin* pluginInstance;

void init(Plugin* p) {
    pluginInstance = p;
    p->addModel(modelSusquehanna);
    p->addModel(modelSchuylkill);
    p->addModel(modelLehigh);
    p->addModel(modelAllegheny);
    p->addModel(modelMonongahela);
    p->addModel(modelLackawanna);
    p->addModel(modelBrandywine);
    p->addModel(modelJuniata);
    p->addModel(modelConestoga);
    p->addModel(modelTioga);
    p->addModel(modelClarion);
    p->addModel(modelKiski);
    p->addModel(modelLoyalsock);
    p->addModel(modelSwatara);
}
