import { Object3D, PlaneGeometry, MeshStandardMaterial, Mesh, DoubleSide } from 'three' // TextureLoader,
import { createProvinceMesh } from './province'

export const createChinaMesh = () => {
  const chinaObj = new Object3D();
  // const texture = new TextureLoader().load(earthImg);
  // const lightMap = new TextureLoader().load(earthLight);
  const planeGeometry = new PlaneGeometry(500, 225);

  planeGeometry.rotateX(Math.PI / 2);
  const planeMaterial = new MeshStandardMaterial({
    // map: texture,
    // lightMap: lightMap,
    // color: '#ffffff',
    side: DoubleSide,
  })
  const planeMesh = new Mesh(planeGeometry, planeMaterial);
  chinaObj.add(planeMesh)
  const { provinceObj, provinceLine } = createProvinceMesh()
  chinaObj.add(provinceObj)
  chinaObj.add(provinceLine)

  return { chinaObj }
}
